// SharedFrameStore.swift - reader side of the shared memory frame region.
//
// Mirrors core/include/vcam/SharedFrameProtocol.h (the writer is
// core/src/ShmFrameWriter.cpp): a 64 byte header followed by the YUYV
// payload, guarded by a seqlock.
//
// Layout of the 64 byte header (little endian):
//   [0]  u32 magic   (0x4D414356 'VCAM')
//   [4]  u32 version (1)
//   [8]  u32 width
//   [12] u32 height
//   [16] u32 stride (= width * 2 for YUYV)
//   [20] u32 format  (1 = YUYV)
//   [24] u32 reserved
//   [28] u32 reserved
//   [32] u32 frameSeq   (seqlock: even = stable, odd = being written)
//   [36] u32 frameCount
//   [40] u64 timestampNs
//   [48] ... padding up to 64

import Foundation

enum SharedFrame {
    static let posixName = "virtualcamera_frame"
    static let headerSize = 64
    static let maxFrameBytes = 1920 * 1080 * 2
    static let regionBytes = headerSize + maxFrameBytes
    static let magic: UInt32 = 0x4D41_4356
    static let version: UInt32 = 1
    static let pixelFormatYuyv: UInt32 = 1
    static let defaultWidth: Int32 = 1280
    static let defaultHeight: Int32 = 720
    static let defaultFps: Int32 = 30
}

// OSAtomic is deprecated but still functional; wrapped here so the
// deprecation warning does not leak into every call site. The frame header
// stores UInt32; OSAtomicAdd32 wants an Int32 pointer, so re-bind it.
@inline(__always)
private func atomicLoad32(_ pointer: UnsafeMutablePointer<UInt32>) -> UInt32 {
    let int32Pointer = UnsafeMutableRawPointer(pointer)
        .assumingMemoryBound(to: Int32.self)
    return UInt32(OSAtomicAdd32(0, int32Pointer))
}

// The SDK imports shm_open as a variadic C function on Apple platforms,
// which Swift rejects ("Variadic function is unavailable"). Bind the real
// symbol with the correct prototype instead.
@_silgen_name("shm_open")
private func shmOpenFixed(_ name: UnsafePointer<CChar>, _ oflag: Int32,
                          _ mode: mode_t) -> Int32

final class SharedFrameStore {

    private var fd: Int32 = -1
    private var region: UnsafeMutableRawPointer?

    deinit {
        close()
    }

    var isOpen: Bool { region != nil }

    /// Open (or re-open) the shared memory region. Returns false while the
    /// controller application is not running; call periodically.
    @discardableResult
    func open() -> Bool {
        if region != nil {
            return true
        }
        let descriptor = shmOpenFixed(SharedFrame.posixName,
                                      O_RDWR,
                                      mode_t(0o666))
        guard descriptor >= 0 else {
            return false
        }
        let mapped = mmap(nil, SharedFrame.regionBytes,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED, descriptor, 0)
        guard let pointer = mapped, pointer != MAP_FAILED else {
            Darwin.close(descriptor)
            return false
        }
        fd = descriptor
        region = pointer
        return true
    }

    func close() {
        if let region = region {
            munmap(region, SharedFrame.regionBytes)
        }
        region = nil
        if fd >= 0 {
            Darwin.close(fd)
            fd = -1
        }
    }

    // MARK: header access

    /// Width/height from the live header, or nil when the controller is
    /// not running.
    func readDimensions() -> (width: Int32, height: Int32)? {
        guard let region = region else { return nil }
        let bytes = region.assumingMemoryBound(to: UInt32.self)
        guard bytes[0] == SharedFrame.magic,
              bytes[1] == SharedFrame.version else {
            return nil
        }
        let width = Int32(bytes[2])
        let height = Int32(bytes[3])
        guard width > 0, height > 0,
              width * height * 2 <= SharedFrame.maxFrameBytes else {
            return nil
        }
        return (width, height)
    }

    // MARK: frame reading (seqlock)

    /// Copy the latest frame into `destination` (which must hold
    /// width * height * 2 bytes). Returns false when no valid frame is
    /// available.
    func readLatest(into destination: UnsafeMutableRawPointer,
                    capacity: Int) -> Bool {
        guard let region = region else { return false }
        let header = region.assumingMemoryBound(to: UInt32.self)
        guard header[0] == SharedFrame.magic,
              header[1] == SharedFrame.version,
              header[5] == SharedFrame.pixelFormatYuyv else {
            return false
        }
        for _ in 0..<4 {
            let seq1 = atomicLoad32(&header[8])
            if seq1 & 1 != 0 {
                usleep(100)
                continue
            }
            let width = Int32(header[2])
            let height = Int32(header[3])
            let stride = Int32(header[4])
            let frameBytes = Int(stride) * Int(height)
            guard width > 0, height > 0, stride == width * 2,
                  frameBytes <= capacity else {
                return false
            }
            memcpy(destination,
                   region + SharedFrame.headerSize,
                   frameBytes)
            let seq2 = atomicLoad32(&header[8])
            if seq1 == seq2 {
                return true
            }
        }
        return false
    }
}

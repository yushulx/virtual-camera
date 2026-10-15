// StreamSource.swift - the CMIOExtensionStreamSource: advertises the
// format and serves frames from the shared memory region (test pattern
// when the controller is not running).

import CoreMediaIO
import CoreMedia
import CoreVideo
import Foundation

final class StreamSource: NSObject, CMIOExtensionStreamSource {

    var stream: CMIOExtensionStream?

    private var width: Int32 = SharedFrame.defaultWidth
    private var height: Int32 = SharedFrame.defaultHeight
    private var fps: Int32 = SharedFrame.defaultFps

    private let frameStore = SharedFrameStore()
    private var frameBuffer: UnsafeMutableRawPointer?
    private var timer: DispatchSourceTimer?
    private let queue = DispatchQueue(label: "com.dynamsoft.virtualcamera.extension")
    private var sequenceNumber: Int64 = 0
    private var frameIndex: Int = 0

    deinit {
        free(frameBuffer)
    }

    // MARK: format

    private func refreshFormat() {
        // Re-open lazily so the store follows the controller lifecycle.
        if !frameStore.isOpen, !frameStore.open() {
            frameStore.close() // drop a possibly stale mapping
            _ = frameStore.open()
        }
        if let dimensions = frameStore.readDimensions() {
            width = dimensions.width
            height = dimensions.height
        } else {
            width = SharedFrame.defaultWidth
            height = SharedFrame.defaultHeight
        }
    }

    private func makeFormatDescription() -> CMVideoFormatDescription? {
        var formatDescription: CMVideoFormatDescription?
        // 'yuvs' = YUYV 4:2:2 byte order (Y0 Cb Y1 Cr) - what we read from
        // the shared memory region.
        let yuvs: FourCharCode = 0x79757673
        let status = CMVideoFormatDescriptionCreate(
            allocator: kCFAllocatorDefault,
            codecType: CMVideoCodecType(yuvs),
            width: width,
            height: height,
            extensions: nil,
            formatDescriptionOut: &formatDescription)
        guard status == noErr else {
            return nil
        }
        return formatDescription
    }

    // MARK: CMIOExtensionStreamSource

    func streamProperties(for stream: CMIOExtensionStream)
        -> CMIOExtensionStreamProperties {
        let properties = CMIOExtensionStreamProperties()
        properties.name = "Virtual Camera"
        properties.model = "Virtual Camera"
        if let format = makeFormatDescription() {
            properties.streamFormat = format
        }
        return properties
    }

    func setStreamProperties(_ properties: CMIOExtensionStreamProperties,
                             for stream: CMIOExtensionStream) {
        // No writable properties.
    }

    func supportedFormatDescriptions(for stream: CMIOExtensionStream)
        -> [CMVideoFormatDescription] {
        refreshFormat()
        return [makeFormatDescription()].compactMap { $0 }
    }

    func stream(_ stream: CMIOExtensionStream,
                didSetFormatDescription formatDescription: CMFormatDescription?) {
        // The system picked the (only) format we advertise.
    }

    func startStream() throws {
        refreshFormat()
        if frameBuffer == nil {
            frameBuffer = malloc(SharedFrame.maxFrameBytes)
        }
        guard frameBuffer != nil else {
            throw NSError(domain: NSOSStatusErrorDomain, code: Int(noErr))
        }

        sequenceNumber = 0
        frameIndex = 0

        let intervalNs = UInt64(1_000_000_000) / UInt64(max(1, fps))
        let timer = DispatchSource.makeTimerSource(queue: queue)
        timer.schedule(deadline: .now(),
                       repeating: .nanoseconds(intervalNs))
        timer.setEventHandler { [weak self] in
            self?.sendFrame()
        }
        timer.resume()
        self.timer = timer
    }

    func stopStream() {
        timer?.cancel()
        timer = nil
    }

    // MARK: frame delivery

    private func sendFrame() {
        guard let stream = stream else { return }
        guard let format = makeFormatDescription() else { return }
        let frameBytes = Int(width) * Int(height) * 2
        guard frameBytes <= SharedFrame.maxFrameBytes, frameBytes > 0 else {
            return
        }
        guard let buffer = frameBuffer else { return }

        // Grab the latest frame; fall back to the moving test pattern so
        // users can verify the camera even without the controller running.
        if !frameStore.isOpen {
            _ = frameStore.open()
        }
        let gotFrame: Bool
        if frameStore.isOpen {
            if !frameStore.readLatest(into: buffer,
                                      capacity: SharedFrame.maxFrameBytes) {
                gotFrame = false
            } else {
                gotFrame = true
            }
        } else {
            gotFrame = false
        }
        if !gotFrame {
            TestPattern.draw(into: buffer,
                             width: Int(width),
                             height: Int(height),
                             frameIndex: frameIndex)
        }
        frameIndex += 1

        // Wrap the bytes in a CMSampleBuffer (copied into malloc'd memory
        // owned by the block buffer through the default allocator).
        var blockBuffer: CMBlockBuffer?
        let memory = malloc(frameBytes)!
        memcpy(memory, buffer, frameBytes)
        let blockStatus = CMBlockBufferCreateWithMemoryBlock(
            allocator: kCFAllocatorDefault,
            memoryBlock: memory,
            blockLength: frameBytes,
            blockAllocator: kCFAllocatorDefault,
            customBlockSource: nil,
            offsetToData: 0,
            dataLength: frameBytes,
            flags: 0,
            blockBufferOut: &blockBuffer)
        guard blockStatus == kCMBlockBufferNoErr, let block = blockBuffer else {
            free(memory)
            return
        }

        var timing = CMSampleTimingInfo(
            duration: CMTime(value: 1, timescale: CMTimeScale(max(1, fps))),
            presentationTimeStamp: CMTime(
                value: CMTimeValue(frameIndex),
                timescale: CMTimeScale(max(1, fps))),
            decodeTimeStamp: .invalid)

        var sampleBuffer: CMSampleBuffer?
        let sampleStatus = CMSampleBufferCreate(
            allocator: kCFAllocatorDefault,
            dataBuffer: block,
            dataReady: true,
            makeDataReadyCallback: nil,
            getByteHandler: nil,
            numSamples: 1,
            numSampleTimingEntries: 1,
            sampleTimingArray: &timing,
            numSampleSizeEntries: 0,
            sampleSizeArray: nil,
            sampleBufferOut: &sampleBuffer)
        guard sampleStatus == noErr, let sample = sampleBuffer else {
            return
        }

        sequenceNumber += 1
        stream.sendFrame(CMIOExtensionSampleBuffer(sampleBuffer: sample,
                                                   sequenceNumber: sequenceNumber))
    }
}

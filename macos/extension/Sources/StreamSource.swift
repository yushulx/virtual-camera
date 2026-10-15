// StreamSource.swift - the CMIOExtensionStreamSource: advertises the stream
// format and serves frames from the shared memory region (moving test
// pattern when the controller application is not running).

import CoreMedia
import CoreMediaIO
import CoreVideo
import Foundation

final class StreamSource: NSObject, CMIOExtensionStreamSource {

    private(set) var stream: CMIOExtensionStream!

    private var width: Int32 = SharedFrame.defaultWidth
    private var height: Int32 = SharedFrame.defaultHeight
    private let fps: Int32 = SharedFrame.defaultFps

    private var formatDescription: CMFormatDescription?
    private var pixelBufferPool: CVPixelBufferPool?
    private var frameBuffer: UnsafeMutableRawPointer?

    private let frameStore = SharedFrameStore()
    private var timer: DispatchSourceTimer?
    private let queue = DispatchQueue(label: "com.dynamsoft.virtualcamera.extension")
    private var frameIndex = 0

    init(localizedName: String, streamID: UUID, clientQueue: DispatchQueue?) {
        super.init()
        refreshFormat()
        stream = CMIOExtensionStream(localizedName: localizedName,
                                     streamID: streamID,
                                     direction: .source,
                                     clockType: .hostTime,
                                     source: self)
    }

    deinit {
        timer?.cancel()
        free(frameBuffer)
    }

    // MARK: format

    /// Re-read the controller's configuration from the shared memory region
    /// and (re)build the format description and pixel buffer pool.
    private func refreshFormat() {
        if !frameStore.isOpen, !frameStore.open() {
            frameStore.close() // drop a stale mapping and retry below
            _ = frameStore.open()
        }
        if let dimensions = frameStore.readDimensions() {
            width = dimensions.width
            height = dimensions.height
        } else {
            width = SharedFrame.defaultWidth
            height = SharedFrame.defaultHeight
        }

        var description: CMFormatDescription?
        // 'yuvs' = YUYV 4:2:2 byte order (Y0 Cb Y1 Cr) - what the shared
        // memory region contains.
        let yuvs: FourCharCode = 0x79757673
        let status = CMVideoFormatDescriptionCreate(
            allocator: kCFAllocatorDefault,
            codecType: CMVideoCodecType(yuvs),
            width: width,
            height: height,
            extensions: nil,
            formatDescriptionOut: &description)
        guard status == noErr, let format = description else {
            return
        }
        formatDescription = format

        var pool: CVPixelBufferPool?
        let attributes: NSDictionary = [
            kCVPixelBufferWidthKey: width,
            kCVPixelBufferHeightKey: height,
            kCVPixelBufferPixelFormatTypeKey: yuvs,
            kCVPixelBufferIOSurfacePropertiesKey: NSDictionary(),
        ]
        CVPixelBufferPoolCreate(kCFAllocatorDefault, nil, attributes, &pool)
        pixelBufferPool = pool

        if frameBuffer == nil {
            frameBuffer = malloc(SharedFrame.maxFrameBytes)
        }
    }

    private var streamFormat: CMIOExtensionStreamFormat {
        let maxFrameDuration = CMTime(value: 1, timescale: CMTimeScale(max(1, fps)))
        return CMIOExtensionStreamFormat(formatDescription: formatDescription!,
                                         maxFrameDuration: maxFrameDuration,
                                         minFrameDuration: maxFrameDuration,
                                         validFrameDurations: nil)
    }

    // MARK: CMIOExtensionStreamSource

    var formats: [CMIOExtensionStreamFormat] {
        formatDescription != nil ? [streamFormat] : []
    }

    var activeFormatIndex = 0

    var availableProperties: Set<CMIOExtensionProperty> {
        [.streamActiveFormatIndex, .streamFrameDuration]
    }

    func streamProperties(forProperties properties: Set<CMIOExtensionProperty>)
        throws -> CMIOExtensionStreamProperties {
        let streamProperties = CMIOExtensionStreamProperties(dictionary: [:])
        if properties.contains(.streamActiveFormatIndex) {
            streamProperties.activeFormatIndex = activeFormatIndex
        }
        if properties.contains(.streamFrameDuration) {
            streamProperties.frameDuration =
                CMTime(value: 1, timescale: CMTimeScale(max(1, fps)))
        }
        return streamProperties
    }

    func setStreamProperties(_ streamProperties: CMIOExtensionStreamProperties) throws {
        if let index = streamProperties.activeFormatIndex {
            activeFormatIndex = index
        }
    }

    func authorizedToStartStream(for client: CMIOExtensionClient) -> Bool {
        true
    }

    func startStream() throws {
        refreshFormat()
        guard formatDescription != nil, pixelBufferPool != nil,
              frameBuffer != nil else {
            throw NSError(domain: "com.dynamsoft.virtualcamera",
                          code: -1,
                          userInfo: [NSLocalizedDescriptionKey:
                                "Could not prepare the stream format"])
        }

        frameIndex = 0
        let intervalNs = 1_000_000_000 / Int(max(1, fps))
        let timer = DispatchSource.makeTimerSource(queue: queue)
        timer.schedule(deadline: .now(), repeating: .nanoseconds(intervalNs))
        timer.setEventHandler { [weak self] in
            self?.sendFrame()
        }
        timer.resume()
        self.timer = timer
    }

    func stopStream() throws {
        timer?.cancel()
        timer = nil
    }

    // MARK: frame delivery

    private func sendFrame() {
        guard let stream = stream, let format = formatDescription,
              let pool = pixelBufferPool, let buffer = frameBuffer else {
            return
        }
        let frameBytes = Int(width) * Int(height) * 2
        guard frameBytes > 0, frameBytes <= SharedFrame.maxFrameBytes else {
            return
        }

        // Grab the latest frame; fall back to the moving test pattern so the
        // camera stays usable without the controller application.
        if !frameStore.isOpen {
            _ = frameStore.open()
        }
        var gotFrame = false
        if frameStore.isOpen {
            gotFrame = frameStore.readLatest(into: buffer,
                                             capacity: SharedFrame.maxFrameBytes)
        }
        if !gotFrame {
            TestPattern.draw(into: buffer,
                             width: Int(width),
                             height: Int(height),
                             frameIndex: frameIndex)
        }
        frameIndex += 1

        // Copy into a pixel buffer from the pool (rows may be padded).
        var pixelBuffer: CVPixelBuffer?
        CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool, &pixelBuffer)
        guard let imageBuffer = pixelBuffer else {
            return
        }
        CVPixelBufferLockBaseAddress(imageBuffer, [])
        if let base = CVPixelBufferGetBaseAddress(imageBuffer) {
            let dstStride = CVPixelBufferGetBytesPerRow(imageBuffer)
            let srcStride = Int(width) * 2
            if dstStride == srcStride {
                memcpy(base, buffer, frameBytes)
            } else {
                for row in 0..<Int(height) {
                    memcpy(base + row * dstStride,
                           buffer + row * srcStride,
                           srcStride)
                }
            }
        }
        CVPixelBufferUnlockBaseAddress(imageBuffer, [])

        var timing = CMSampleTimingInfo(
            duration: CMTime(value: 1, timescale: CMTimeScale(max(1, fps))),
            presentationTimeStamp: CMClockGetTime(CMClockGetHostTimeClock()),
            decodeTimeStamp: .invalid)
        var sampleBuffer: CMSampleBuffer?
        let status = CMSampleBufferCreateForImageBuffer(
            allocator: kCFAllocatorDefault,
            imageBuffer: imageBuffer,
            dataReady: true,
            makeDataReadyCallback: nil,
            refcon: nil,
            formatDescription: format,
            sampleTiming: &timing,
            sampleBufferOut: &sampleBuffer)
        guard status == noErr, let sb = sampleBuffer else {
            return
        }

        let hostTimeNs = UInt64(
            CMClockGetTime(CMClockGetHostTimeClock()).seconds
                * Double(NSEC_PER_SEC))
        stream.send(sb, discontinuity: [], hostTimeInNanoseconds: hostTimeNs)
    }
}

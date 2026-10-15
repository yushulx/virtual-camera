// DeviceSource.swift - the CMIOExtensionDeviceSource: configures the
// virtual camera device and owns its stream.

import CoreMediaIO
import Foundation
import IOKit.audio

final class DeviceSource: NSObject, CMIOExtensionDeviceSource {

    /// Stable device identity so camera permissions and selections survive
    /// reinstalls. (Also referenced in docs/INSTALL.md.)
    static let deviceID = UUID(uuidString: "3E7C9B54-8A21-4F6D-9B3E-1C5D7A9F2E48")!

    private(set) var device: CMIOExtensionDevice!
    private let streamSource: StreamSource

    init(clientQueue: DispatchQueue? = nil) {
        streamSource = StreamSource(localizedName: "Virtual Camera",
                                    streamID: UUID(),
                                    clientQueue: clientQueue)
        super.init()
        device = CMIOExtensionDevice(localizedName: "Virtual Camera",
                                     deviceID: Self.deviceID,
                                     legacyDeviceID: nil,
                                     source: self)
    }

    /// Attach the stream (called once from main.swift via the provider).
    func prepare() throws {
        try device.addStream(streamSource.stream)
    }

    // MARK: CMIOExtensionDeviceSource

    var availableProperties: Set<CMIOExtensionProperty> {
        [.deviceTransportType, .deviceModel]
    }

    func deviceProperties(forProperties properties: Set<CMIOExtensionProperty>)
        throws -> CMIOExtensionDeviceProperties {
        let deviceProperties = CMIOExtensionDeviceProperties(dictionary: [:])
        if properties.contains(.deviceTransportType) {
            deviceProperties.transportType = kIOAudioDeviceTransportTypeVirtual
        }
        if properties.contains(.deviceModel) {
            deviceProperties.model = "Virtual Camera"
        }
        return deviceProperties
    }

    func setDeviceProperties(_ deviceProperties: CMIOExtensionDeviceProperties) throws {
        // No writable device properties.
    }
}

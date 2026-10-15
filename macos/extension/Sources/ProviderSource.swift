// ProviderSource.swift - the CMIOExtensionProviderSource: manages the
// provider-wide properties and publishes the virtual camera device.

import CoreMediaIO
import Foundation

final class ProviderSource: NSObject, CMIOExtensionProviderSource {

    private(set) var provider: CMIOExtensionProvider!
    private let deviceSource: DeviceSource

    let manufacturer = "Dynamsoft"

    init(clientQueue: DispatchQueue? = nil) {
        deviceSource = DeviceSource(clientQueue: clientQueue)
        super.init()
        provider = CMIOExtensionProvider(source: self, clientQueue: clientQueue)
    }

    /// Publish the device (called once from main.swift).
    func prepare() throws {
        try provider.addDevice(deviceSource.device)
    }

    // MARK: CMIOExtensionProviderSource

    func connect(to client: CMIOExtensionClient) throws {
        // Every client may use the camera.
    }

    func disconnect(from client: CMIOExtensionClient) {
        // Nothing to clean up.
    }

    var availableProperties: Set<CMIOExtensionProperty> {
        [.providerManufacturer]
    }

    func providerProperties(forProperties properties: Set<CMIOExtensionProperty>)
        throws -> CMIOExtensionProviderProperties {
        let providerProperties = CMIOExtensionProviderProperties(dictionary: [:])
        if properties.contains(.providerManufacturer) {
            providerProperties.manufacturer = manufacturer
        }
        return providerProperties
    }

    func setProviderProperties(_ providerProperties: CMIOExtensionProviderProperties) throws {
        // No writable provider properties.
    }
}

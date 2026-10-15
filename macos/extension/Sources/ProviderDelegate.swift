// ProviderDelegate.swift - builds and provides the virtual camera device.

import CoreMediaIO
import Foundation

final class ProviderDelegate: NSObject, CMIOExtensionProviderDelegate {

    private let device: CMIOExtensionDevice
    private let streamSource: StreamSource
    private var provider: CMIOExtensionProvider?

    override init() {
        // The stream source reads its frames from the shared memory region
        // written by the VirtualCamera application (and falls back to a
        // built-in test pattern when the app is not running).
        streamSource = StreamSource()

        let stream = CMIOExtensionStream(micaStreamID: CMIOExtensionStreamID(),
                                         localizedName: "Virtual Camera",
                                         delegate: streamSource,
                                         clientQueue: nil)

        let deviceDelegate = DeviceDelegate()
        device = CMIOExtensionDevice(micaDeviceID: CMIOExtensionDeviceID(),
                                     localizedName: "Virtual Camera",
                                     delegate: deviceDelegate,
                                     clientQueue: nil)
        super.init()
        device.connect(stream)
    }

    /// Called from main.swift once the provider exists.
    func attach(provider: CMIOExtensionProvider) {
        self.provider = provider
        provider.connect(device)
    }

    // MARK: CMIOExtensionProviderDelegate

    func provider(_ provider: CMIOExtensionProvider,
                  openSessionFor device: CMIOExtensionDevice) throws {
        // Nothing to do: the device is always available.
    }

    func provider(_ provider: CMIOExtensionProvider,
                  closeSessionFor device: CMIOExtensionDevice) {
        // Nothing to do.
    }

    func provider(_ provider: CMIOExtensionProvider,
                  deviceDidDisconnect device: CMIOExtensionDevice) {
        // The system told us the device went away; it will come back with
        // the next session.
    }
}

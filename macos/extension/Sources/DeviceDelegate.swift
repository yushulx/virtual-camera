// DeviceDelegate.swift - the properties of the virtual camera device.

import CoreMediaIO
import Foundation

final class DeviceDelegate: NSObject, CMIOExtensionDeviceDelegate {

    func deviceProperties(for device: CMIOExtensionDevice)
        -> CMIOExtensionDeviceProperties {
        let properties = CMIOExtensionDeviceProperties()
        properties.name = "Virtual Camera"
        properties.model = "Virtual Camera"
        properties.manufacturer = "Dynamsoft"
        properties.deviceUID = "com.dynamsoft.virtualcamera.device"
        properties.transportType = .virtual
        return properties
    }

    func setDeviceProperties(_ properties: CMIOExtensionDeviceProperties,
                             for device: CMIOExtensionDevice) {
        // The device has no writable properties.
    }
}

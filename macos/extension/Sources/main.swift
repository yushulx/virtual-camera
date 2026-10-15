// main.swift - entry point of the CMIO camera system extension.
//
// The system extension daemon (sysexd) launches this executable, which
// starts the CMIOExtensionProvider service and stays alive. The host
// application (VirtualCamera.app) requests the activation once via
// OSSystemExtensionManager (see app/src/SystemExtensionActivator.mm).

import CoreMediaIO
import Foundation

let providerSource = ProviderSource()

do {
    try providerSource.prepare()
} catch {
    fatalError("Virtual Camera provider could not publish its device: \(error)")
}

CMIOExtensionProvider.startService(provider: providerSource.provider)

dispatchMain()

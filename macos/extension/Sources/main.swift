// main.swift - entry point of the CMIO camera system extension.
//
// The system extension daemon (sysexd) launches this executable, which
// activates the CMIOExtensionProvider and stays alive. The host application
// (VirtualCamera.app) requests the activation once via
// OSSystemExtensionRequest (see app/src/SystemExtensionActivator.mm).

import CoreMediaIO
import Foundation

let delegate = ProviderDelegate()
let provider = CMIOExtensionProvider(localizedName: "Virtual Camera",
                                     delegate: delegate,
                                     clientQueue: nil)
delegate.attach(provider: provider)

do {
    try provider.activate()
} catch {
    fatalError("Virtual Camera extension failed to activate: \(error)")
}

dispatchMain()

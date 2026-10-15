// SystemExtensionActivator.h - requests activation of the macOS CMIO camera
// extension from the host application (OSSystemExtensionRequest).
//
// Pure C++ on the interface; the macOS implementation lives in
// SystemExtensionActivator.mm. On other platforms activate() fails.

#pragma once

#include <QObject>
#include <QString>

namespace vcam {

class ExtensionActivator : public QObject
{
    Q_OBJECT

public:
    explicit ExtensionActivator(QObject* parent = nullptr);

    // `extensionIdentifier` must be prefixed with the application's bundle
    // identifier (com.dynamsoft.virtualcamera.*).
    void activate(const QString& extensionIdentifier);

signals:
    void finished(bool ok, const QString& message);
};

} // namespace vcam

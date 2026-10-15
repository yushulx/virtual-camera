#include "SystemExtensionActivator.h"

#if defined(Q_OS_MACOS)

extern "C" void vcamMacActivateSystemExtension(
    const char* identifier, void (*reply)(int ok, const char* message, void* userData),
    void* userData);

#endif

namespace vcam {

ExtensionActivator::ExtensionActivator(QObject* parent)
    : QObject(parent)
{
}

void ExtensionActivator::activate(const QString& extensionIdentifier)
{
#if defined(Q_OS_MACOS)
    vcamMacActivateSystemExtension(
        extensionIdentifier.toUtf8().constData(),
        [](int ok, const char* message, void* userData) {
            auto* self = static_cast<ExtensionActivator*>(userData);
            emit self->finished(ok != 0,
                                QString::fromUtf8(message ? message : ""));
        },
        this);
#else
    Q_UNUSED(extensionIdentifier);
    emit finished(false,
                  QStringLiteral("System extensions are only supported on macOS."));
#endif
}

} // namespace vcam

// cli - headless controller for testing, servers and CI.
//
//   virtual-camera-cli --pattern --width 1280 --height 720 --fps 30 --duration 10

#include "vcam/CameraController.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QTimer>

#include <atomic>
#include <csignal>
#include <cstdio>

#if defined(Q_OS_LINUX)
#include "vcam/V4L2Writer.h"
#endif

namespace {

std::atomic<bool> g_stopRequested{false};

void handleSignal(int)
{
    g_stopRequested.store(true);
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("virtual-camera-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Feed a virtual camera without the GUI."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sourceOption(
        {QStringLiteral("s"), QStringLiteral("source")},
        QStringLiteral("Video or image file to loop. Omit for the test pattern."),
        QStringLiteral("file"));
    QCommandLineOption widthOption(QStringLiteral("width"),
                                   QStringLiteral("Frame width."),
                                   QStringLiteral("pixels"), QStringLiteral("1280"));
    QCommandLineOption heightOption(QStringLiteral("height"),
                                    QStringLiteral("Frame height."),
                                    QStringLiteral("pixels"), QStringLiteral("720"));
    QCommandLineOption fpsOption(QStringLiteral("fps"),
                                 QStringLiteral("Frames per second."),
                                 QStringLiteral("fps"), QStringLiteral("30"));
    QCommandLineOption durationOption(
        QStringLiteral("duration"),
        QStringLiteral("Stop after N seconds (0 = run until Ctrl+C)."),
        QStringLiteral("seconds"), QStringLiteral("0"));
    QCommandLineOption patternOption(
        QStringLiteral("pattern"),
        QStringLiteral("Use the built-in moving test pattern (default when no source is given)."));
    QCommandLineOption listDevicesOption(QStringLiteral("list-devices"),
                                         QStringLiteral("List video devices and exit (Linux)."));

    parser.addOptions({sourceOption, widthOption, heightOption, fpsOption,
                       durationOption, patternOption, listDevicesOption});
    parser.process(app);

#if defined(Q_OS_LINUX)
    if (parser.isSet(listDevicesOption)) {
        const QList<vcam::V4l2DeviceInfo> devices = vcam::listV4l2Devices();
        if (devices.isEmpty()) {
            std::printf("no video devices found\n");
            return 1;
        }
        for (const vcam::V4l2DeviceInfo& info : devices) {
            std::printf("%s  card=\"%s\"  capture=%s output=%s\n",
                        qPrintable(info.device), qPrintable(info.card),
                        info.canCapture ? "yes" : "no",
                        info.canOutput ? "yes" : "no");
        }
        return 0;
    }
#else
    Q_UNUSED(listDevicesOption);
#endif

    vcam::CameraConfig cfg;
    cfg.sourcePath = parser.value(sourceOption);
    cfg.width = parser.value(widthOption).toInt();
    cfg.height = parser.value(heightOption).toInt();
    cfg.fps = parser.value(fpsOption).toInt();
    if (!cfg.isValid()) {
        std::fprintf(stderr, "invalid --width/--height/--fps\n");
        return 2;
    }

    vcam::CameraController controller;
    QString error;
    if (!controller.start(cfg, &error)) {
        std::fprintf(stderr, "failed to start: %s\n", qPrintable(error));
        return 3;
    }
    std::printf("virtual camera running: %dx%d @ %d fps\n", cfg.width, cfg.height,
                cfg.fps);

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    // Poll the signal flag so Ctrl+C works headless.
    QTimer watchdog;
    watchdog.setInterval(200);
    QObject::connect(&watchdog, &QTimer::timeout, &app,
                     [&]() {
                         if (g_stopRequested.load()) {
                             QCoreApplication::quit();
                         }
                     });
    watchdog.start();

    if (duration > 0) {
        QTimer::singleShot(duration * 1000, &app, &QCoreApplication::quit);
    }

    const int code = app.exec();
    controller.stop();
    std::printf("virtual camera stopped\n");
    return code;
}

// test_core - unit tests for the conversion, configuration and the shared
// memory protocol. Plain asserts (no Qt Test dependency), run via ctest.

#include "vcam/CameraController.h"
#include "vcam/Config.h"
#include "vcam/ImageSource.h"
#include "vcam/MediaSource.h"
#include "vcam/PatternSource.h"
#include "vcam/ShmFrameWriter.h"
#include "vcam/SharedFrameProtocol.h"
#include "vcam/YuyvConverter.h"

#include <QColor>
#include <QGuiApplication>
#include <QTemporaryDir>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace vcam;

#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!((a) == (b))) {                                                 \
            ++g_failures;                                                    \
            std::printf("FAILED %s:%d: %s == %s\n", __FILE__, __LINE__, #a,  \
                         #b);                                                \
        }                                                                    \
    } while (0)

QImage solidImage(int width, int height, QColor color)
{
    QImage image(width, height, QImage::Format_RGB888);
    image.fill(color);
    return image;
}

void testYuyvBasics()
{
    const QImage black = solidImage(4, 2, QColor(0, 0, 0));
    const QImage white = solidImage(4, 2, QColor(255, 255, 255));
    const QImage red = solidImage(4, 2, QColor(255, 0, 0));

    std::vector<uint8_t> yuyv(4 * 2 * 2);
    CHECK(convertImageToYuyv(black, yuyv.data(), yuyv.size()));
    // Black: Y=16, U=V=128.
    CHECK_EQ(yuyv[0], 16);
    CHECK_EQ(yuyv[1], 128);
    CHECK_EQ(yuyv[2], 16);
    CHECK_EQ(yuyv[3], 128);

    CHECK(convertImageToYuyv(white, yuyv.data(), yuyv.size()));
    // White: Y=235, U=V=128.
    CHECK_EQ(yuyv[0], 235);
    CHECK_EQ(yuyv[1], 128);

    CHECK(convertImageToYuyv(red, yuyv.data(), yuyv.size()));
    // Red: Y~81, V>128, U<128 (BT.601).
    CHECK(yuyv[0] > 60 && yuyv[0] < 100);
    CHECK(yuyv[1] < 128);  // U
    CHECK(yuyv[3] > 160);  // V

    // Capacity guard.
    CHECK(!convertImageToYuyv(black, yuyv.data(), 8));
    CHECK(!convertImageToYuyv(QImage(), yuyv.data(), yuyv.size()));

    // Round trip does not explode.
    const QImage back = convertYuyvToImage(yuyv.data(), 4, 2, yuyv.size());
    CHECK(!back.isNull());
    CHECK_EQ(back.width(), 4);
    CHECK_EQ(back.height(), 2);
}

void testYuyvOddWidth()
{
    // 3 pixels wide: the last (odd) pixel duplicates for chroma.
    const QImage image = solidImage(3, 1, QColor(30, 30, 30));
    std::vector<uint8_t> yuyv(3 * 1 * 2);
    CHECK(convertImageToYuyv(image, yuyv.data(), yuyv.size()));
    CHECK_EQ(yuyv[2], yuyv[0]); // Y1 uses the repeated third pixel
}

void testPatternSource()
{
    vcam::PatternSource source;
    QImage frame;
    CHECK(source.latestFrame(64, 48, &frame));
    CHECK(!frame.isNull());
    CHECK_EQ(frame.width(), 64);
    CHECK_EQ(frame.height(), 48);
    QImage second;
    CHECK(source.latestFrame(64, 48, &second));
    CHECK(source.lastError().isEmpty());
}

void testImageSource()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("test.png"));
    QImage original(40, 30, QImage::Format_RGB32);
    original.fill(QColor(10, 200, 30));
    CHECK(original.save(path));

    vcam::ImageSource source(path);
    QImage frame;
    CHECK(source.latestFrame(80, 60, &frame));  // cached scaled copy works
    CHECK_EQ(frame.width(), 80);
    CHECK_EQ(frame.height(), 60);

    vcam::ImageSource missing(dir.filePath(QStringLiteral("nope.png")));
    CHECK(!missing.latestFrame(10, 10, &frame));
    CHECK(!missing.lastError().isEmpty());
}

void testConfigRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.json"));
    qputenv("VCAM_CONFIG_PATH", path.toUtf8());

    vcam::CameraConfig cfg;
    cfg.sourcePath = QStringLiteral("/some/movie.mp4");
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.fps = 25;
    cfg.loop = false;
    cfg.save();

    const vcam::CameraConfig loaded = vcam::CameraConfig::load();
    CHECK_EQ(loaded.sourcePath, cfg.sourcePath);
    CHECK_EQ(loaded.width, 1920);
    CHECK_EQ(loaded.height, 1080);
    CHECK_EQ(loaded.fps, 25);
    CHECK_EQ(loaded.loop, false);

    qunsetenv("VCAM_CONFIG_PATH");
}

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void testSharedFrameWriter()
{
    vcam::ShmFrameWriter writer;
    vcam::CameraConfig cfg;
    cfg.width = 64;
    cfg.height = 48;
    cfg.fps = 30;

    QString error;
    if (!writer.start(cfg, &error)) {
#if defined(Q_OS_MACOS)
        // Some CI environments restrict POSIX shared memory; report the
        // reason without failing the whole suite there.
        std::printf("WARNING: shared frame memory unavailable: %s\n",
                    qPrintable(error));
        return;
#else
        std::printf("writer.start error: %s\n", qPrintable(error));
#endif
    }
    CHECK(writer.isRunning());

    const QImage frame = solidImage(64, 48, QColor(255, 255, 255));
    std::vector<uint8_t> yuyv(64 * 48 * 2);
    CHECK(convertImageToYuyv(frame, yuyv.data(), yuyv.size()));
    CHECK(writer.writeFrame(yuyv.data(), 64, 48));

    // Read it back the way the drivers do.
    const uint8_t* region = nullptr;
# if defined(Q_OS_WIN)
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE,
        QString::fromLatin1(vcam::kShmWindowsName).toStdWString().c_str());
    CHECK(mapping != nullptr);
    if (mapping) {
        region = static_cast<const uint8_t*>(
            MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, vcam::kSharedRegionBytes));
    }
# else
    const int fd = shm_open(vcam::kShmPosixName, O_RDWR, 0666);
    CHECK(fd >= 0);
    if (fd >= 0) {
        region = static_cast<const uint8_t*>(
            mmap(nullptr, vcam::kSharedRegionBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    }
# endif
    CHECK(region != nullptr);
    if (region) {
        const auto* header =
            reinterpret_cast<const vcam::SharedFrameHeader*>(region);
        CHECK_EQ(header->magic, vcam::kSharedFrameMagic);
        CHECK_EQ(header->version, vcam::kSharedFrameVersion);
        CHECK_EQ(header->width, 48u + 16u); // 64
        CHECK_EQ(header->height, 48u);
        CHECK_EQ(header->format, vcam::kPixelFormatYuyv);
        // Payload: white -> Y=235 U=V=128.
        const uint8_t* payload = region + vcam::kSharedHeaderSize;
        CHECK_EQ(payload[0], 235);
        CHECK_EQ(payload[1], 128);

# if defined(Q_OS_WIN)
        UnmapViewOfFile(const_cast<uint8_t*>(region));
        CloseHandle(mapping);
# else
        munmap(const_cast<uint8_t*>(region), vcam::kSharedRegionBytes);
        close(fd);
# endif
    }

    writer.stop();
    CHECK(!writer.isRunning());
}
#endif

} // namespace

int main(int argc, char** argv)
{
    // QGuiApplication: PatternSource draws text with QPainter, which needs
    // the font database. Use the offscreen platform in headless runs.
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("virtual-camera-tests"));

    testYuyvBasics();
    testYuyvOddWidth();
    testPatternSource();
    testImageSource();
    testConfigRoundtrip();
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    testSharedFrameWriter();
#endif

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

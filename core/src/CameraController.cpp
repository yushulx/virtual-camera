#include "vcam/CameraController.h"

#include "vcam/FrameWriter.h"
#include "vcam/FolderSource.h"
#include "vcam/ImageSource.h"
#include "vcam/MediaSource.h"
#include "vcam/PatternSource.h"
#include "vcam/ShmFrameWriter.h"
#include "vcam/VideoSource.h"
#include "vcam/YuyvConverter.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <QTimer>

#include <chrono>
#include <vector>

#if defined(Q_OS_LINUX)
#include "vcam/V4L2Writer.h"
#endif

namespace vcam {

CameraController::CameraController(QObject* parent)
    : QObject(parent)
{
}

CameraController::~CameraController()
{
    stop();
}

FrameWriter* CameraController::makeWriter()
{
#if defined(Q_OS_LINUX)
    return new V4L2Writer();
#else
    return new ShmFrameWriter();
#endif
}

std::unique_ptr<MediaSource>
CameraController::makeSource(const CameraConfig& cfg, QString* error)
{
    const QString path = cfg.sourcePath;

    // Multiple selected images: cycle them like a folder slideshow.
    if (cfg.sourceFiles.size() > 1) {
        auto source = std::make_unique<FolderSource>();
        source->setFramesPerImage(std::max(1, cfg.fps * 2)); // ~2 s per image
        if (!source->loadFiles(cfg.sourceFiles, error)) {
            return nullptr;
        }
        return source;
    }

    if (path.isEmpty()) {
        // No file selected: run the built-in test pattern.
        return std::make_unique<PatternSource>();
    }

    // Video or image? Ask QImageReader first: a video file cannot be decoded
    // as an image (with canRead/QImageReader::canRead returning false) and
    // video containers are detected by suffix.
    static const QSet<QString> videoSuffixes = {
        QStringLiteral("mp4"),  QStringLiteral("mov"), QStringLiteral("mkv"),
        QStringLiteral("webm"), QStringLiteral("avi"), QStringLiteral("m4v"),
        QStringLiteral("mpg"),  QStringLiteral("mpeg"), QStringLiteral("ts"),
        QStringLiteral("flv"),  QStringLiteral("wmv"),
    };
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (videoSuffixes.contains(suffix)) {
        auto source = std::make_unique<VideoSource>(path, cfg.loop);
        if (!source->start(error)) {
            return nullptr;
        }
        return source;
    }

    // A directory: cycle all images inside it as a slideshow.
    if (QFileInfo(path).isDir()) {
        auto source = std::make_unique<FolderSource>();
        source->setFramesPerImage(std::max(1, cfg.fps * 2)); // ~2 s per image
        if (!source->load(path, error)) {
            return nullptr;
        }
        return source;
    }

    QImageReader reader(path);
    if (reader.canRead()) {
        return std::make_unique<ImageSource>(path);
    }

    // Unknown suffix but a decodable video: try it as a video anyway.
    if (QFileInfo::exists(path)) {
        auto source = std::make_unique<VideoSource>(path, cfg.loop);
        if (source->start(error)) {
            return source;
        }
    }
    if (error && error->isEmpty()) {
        *error = QStringLiteral("Unrecognized source file: %1").arg(path);
    }
    return nullptr;
}

bool CameraController::start(const CameraConfig& cfg, QString* error)
{
    if (m_running.load()) {
        stop();
    }
    if (!cfg.isValid()) {
        if (error) {
            *error = QStringLiteral("Invalid configuration.");
        }
        return false;
    }

    m_config = cfg;
    m_source = makeSource(cfg, error);
    if (!m_source) {
        return false;
    }

    m_writer.reset(makeWriter());
    if (!m_writer->start(cfg, error)) {
        m_source.reset();
        m_writer.reset();
        return false;
    }

    m_running.store(true);
    m_thread = std::make_unique<std::thread>(&CameraController::runLoop, this);
    emit stateChanged(true);
    return true;
}

void CameraController::stop()
{
    if (!m_running.exchange(false)) {
        return;
    }
    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
    m_thread.reset();
    if (m_writer) {
        m_writer->stop();
    }
    m_writer.reset();
    m_source.reset();
    emit stateChanged(false);
}

void CameraController::runLoop()
{
    std::vector<uint8_t> staging(
        static_cast<size_t>(m_config.width) * m_config.height * 2, 0);
    const auto interval =
        std::chrono::nanoseconds(std::chrono::seconds(1)) / m_config.fps;
    auto deadline = std::chrono::steady_clock::now();
    QImage frame;
    uint64_t frameIndex = 0;
    bool warnedNoFrame = false;

    while (m_running.load()) {
        if (m_source->latestFrame(m_config.width, m_config.height, &frame)) {
            if (convertImageToYuyv(frame, staging.data(), staging.size())) {
                m_writer->writeFrame(staging.data(), m_config.width,
                                      m_config.height);
                ++frameIndex;
                warnedNoFrame = false;
            }
        } else if (!warnedNoFrame) {
            warnedNoFrame = true; // decoding not started yet; keep trying
        }

        deadline += interval;
        const auto now = std::chrono::steady_clock::now();
        if (deadline > now) {
            std::this_thread::sleep_until(deadline);
        } else {
            // Too far behind: do not burst to catch up.
            deadline = now;
        }
    }
}

} // namespace vcam

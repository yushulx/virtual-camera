#include "vcam/VideoSource.h"

#include "vcam/ImageFit.h"

#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

namespace vcam {

VideoSource::VideoSource(const QString& path, bool loop, QObject* parent)
    : QObject(parent), m_loop(loop)
{
    m_player = new QMediaPlayer(this);
    // No audio output is attached: a virtual camera is silent.
    m_player->setSource(QUrl::fromUserInput(path));

    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);

    connect(m_sink, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame& frame) { handleFrame(frame); });

    // -1 = loop forever (Qt 6). Also restart on EndOfMedia as a belt and
    // braces for media backends that ignore the loops property.
    m_player->setLoops(-1);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia && m_loop) {
                    m_player->setPosition(0);
                    m_player->play();
                }
            });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString& str) {
                if (error != QMediaPlayer::NoError) {
                    QMutexLocker lock(&m_mutex);
                    m_error = str;
                }
            });
}

VideoSource::~VideoSource() = default;

bool VideoSource::start(QString* error)
{
    switch (m_player->mediaStatus()) {
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::InvalidMedia:
        if (error) {
            *error = m_error.isEmpty()
                         ? QStringLiteral("The video file cannot be opened.")
                         : m_error;
        }
        return false;
    default:
        break;
    }
    m_player->play();
    return true;
}

void VideoSource::handleFrame(const QVideoFrame& frame)
{
    if (!frame.isValid()) {
        return;
    }
    QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }
    QMutexLocker lock(&m_mutex);
    m_latest = std::move(image);
}

bool VideoSource::latestFrame(int width, int height, QImage* out)
{
    if (out == nullptr) {
        return false;
    }
    QImage frame;
    {
        QMutexLocker lock(&m_mutex);
        if (m_latest.isNull()) {
            return false;
        }
        frame = m_latest;
    }
    if (frame.size() == QSize(width, height)) {
        *out = frame;
        return true;
    }
    // Letterbox rather than stretch, and do it outside the lock so the
    // capture thread is not held up by the fit.
    *out = fitImageToCanvas(frame, width, height, Qt::FastTransformation);
    return true;
}

} // namespace vcam

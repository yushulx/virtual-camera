// VideoSource.h - a video file played in an infinite loop via Qt Multimedia.
//
// QMediaPlayer delivers frames through QVideoSink on the GUI thread; this
// class keeps the latest decoded frame as a QImage (guarded by a mutex) so
// the controller's pacing thread can pick it up at the target frame rate.

#pragma once

#include "vcam/MediaSource.h"

#include <QMutex>
#include <QObject>

class QMediaPlayer;
class QVideoFrame;
class QVideoSink;

namespace vcam {

class VideoSource final : public QObject, public MediaSource
{
    Q_OBJECT

public:
    // Must be created on the GUI thread (QMediaPlayer needs an event loop).
    VideoSource(const QString& path, bool loop, QObject* parent = nullptr);
    ~VideoSource() override;

    bool start(QString* error);
    bool latestFrame(int width, int height, QImage* out) override;
    QString lastError() const override { return m_error; }

private:
    void handleFrame(const QVideoFrame& frame);

    QMediaPlayer* m_player = nullptr;
    QVideoSink* m_sink = nullptr;
    QImage m_latest;
    mutable QMutex m_mutex;
    QString m_error;
    bool m_loop = true;
};

} // namespace vcam

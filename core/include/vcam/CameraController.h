// CameraController.h - glues a MediaSource and a platform FrameWriter
// together and paces the frames at the configured frame rate.

#pragma once

#include "vcam/Config.h"

#include <QObject>
#include <memory>
#include <atomic>
#include <thread>

namespace vcam {

class MediaSource;
class FrameWriter;

class CameraController : public QObject
{
    Q_OBJECT

public:
    explicit CameraController(QObject* parent = nullptr);
    ~CameraController() override;

    // `cfg` is stored; the source is created (video sources are created on
    // the calling/GUI thread) and the pacing thread started.
    bool start(const CameraConfig& cfg, QString* error);
    void stop();

    bool isRunning() const { return m_running.load(); }
    CameraConfig config() const { return m_config; }

    // Access to the source for the GUI preview (may be null before start).
    MediaSource* source() const { return m_source.get(); }

signals:
    void stateChanged(bool running);
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<MediaSource> makeSource(const CameraConfig& cfg,
                                            QString* error);
    void runLoop();
    FrameWriter* makeWriter();

    CameraConfig m_config;
    std::unique_ptr<MediaSource> m_source;
    std::unique_ptr<FrameWriter> m_writer;
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_running{false};
    QString m_lastError;
};

} // namespace vcam

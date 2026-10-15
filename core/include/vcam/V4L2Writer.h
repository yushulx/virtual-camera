// V4L2Writer.h - Linux output: writes YUYV frames into a v4l2loopback
// device node (the "Virtual Camera" is created by the v4l2loopback kernel
// module, installed and configured by the package).

#pragma once

#include "vcam/FrameWriter.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <cstdint>

struct v4l2_format;

namespace vcam {

struct V4l2DeviceInfo
{
    QString device;   // /dev/videoN
    QString card;     // card label, e.g. "Virtual Camera"
    bool canCapture = false;
    bool canOutput = false;
};

// Enumerate /dev/video* nodes with their capabilities (used by the GUI
// status line and the CLI --list-devices).
QList<V4l2DeviceInfo> listV4l2Devices();

class V4L2Writer final : public FrameWriter
{
public:
    V4L2Writer() = default;
    ~V4L2Writer() override;

    bool start(const CameraConfig& cfg, QString* error) override;
    void stop() override;
    bool writeFrame(const uint8_t* data, int width, int height) override;
    bool isRunning() const override { return m_fd >= 0; }
    QString describe() const override;

    // Device to use; when empty, the first node whose card label matches
    // kCardLabel is used. Can be overridden with the VCAM_V4L2_DEVICE
    // environment variable.
    static constexpr const char* kCardLabel = "Virtual Camera";

private:
    QString findDevice(QString* error) const;

    int m_fd = -1;
    int m_width = 0;
    int m_height = 0;
    int m_fps = 30;
    QString m_devicePath;
};

} // namespace vcam

#include "vcam/V4L2Writer.h"

#include <QDir>
#include <QFile>
#include <QScopeGuard>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>

namespace vcam {

namespace {

bool queryDevice(const QString& path, V4l2DeviceInfo* info)
{
    const int fd = open(path.toUtf8().constData(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }
    auto closeFd = qScopeGuard([fd] { close(fd); });

    v4l2_capability caps{};
    if (ioctl(fd, VIDIOC_QUERYCAP, &caps) != 0) {
        return false;
    }
    info->device = path;
    const char* card = reinterpret_cast<const char*>(caps.card);
    info->card = QString::fromLatin1(card,
                                    strnlen(card, sizeof(caps.card)));
    const uint32_t capsFlags = caps.device_caps ? caps.device_caps : caps.capabilities;
    info->canCapture = capsFlags & V4L2_CAP_VIDEO_CAPTURE;
    info->canOutput = capsFlags & V4L2_CAP_VIDEO_OUTPUT;
    return true;
}

} // namespace

QList<V4l2DeviceInfo> listV4l2Devices()
{
    QList<V4l2DeviceInfo> result;
    const QDir dir(QStringLiteral("/dev"));
    const QStringList entries =
        dir.entryList({QStringLiteral("video*")}, QDir::System | QDir::Files);
    for (const QString& entry : entries) {
        V4l2DeviceInfo info;
        const QString path = QStringLiteral("/dev/") + entry;
        if (queryDevice(path, &info)) {
            result.append(info);
        }
    }
    return result;
}

QString V4L2Writer::findDevice(QString* error) const
{
    if (qEnvironmentVariableIsSet("VCAM_V4L2_DEVICE")) {
        const QString override = qEnvironmentVariable("VCAM_V4L2_DEVICE");
        if (QFile::exists(override)) {
            return override;
        }
        if (error) {
            *error = QStringLiteral("VCAM_V4L2_DEVICE=%1 does not exist")
                          .arg(override);
        }
        return QString();
    }

    // Prefer the node labelled "Virtual Camera" (card_label module option
    // written by the package postinst).
    const QList<V4l2DeviceInfo> devices = listV4l2Devices();
    for (const V4l2DeviceInfo& info : devices) {
        if (info.card == QLatin1String(kCardLabel) && info.canOutput) {
            return info.device;
        }
    }
    for (const V4l2DeviceInfo& info : devices) {
        if (info.canOutput) {
            return info.device;
        }
    }
    if (error) {
        *error = QStringLiteral(
                     "No v4l2 output device found. Install and load the "
                     "v4l2loopback module:\n"
                     "  sudo apt install v4l2loopback-dkms\n"
                     "  sudo modprobe v4l2loopback exclusive_caps=1 "
                     "card_label=\"%1\" max_devices=1")
                     .arg(QString::fromLatin1(kCardLabel));
    }
    return QString();
}

V4L2Writer::~V4L2Writer()
{
    stop();
}

bool V4L2Writer::start(const CameraConfig& cfg, QString* error)
{
    if (isRunning()) {
        return true;
    }
    const QString device = findDevice(error);
    if (device.isEmpty()) {
        return false;
    }

    m_fd = open(device.toUtf8().constData(), O_WRONLY);
    if (m_fd < 0) {
        if (error) {
            *error = QStringLiteral("Cannot open %1: %2")
                          .arg(device, QString::fromLatin1(strerror(errno)));
        }
        return false;
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = static_cast<uint32_t>(cfg.width);
    fmt.fmt.pix.height = static_cast<uint32_t>(cfg.height);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = static_cast<uint32_t>(cfg.width * 2);
    fmt.fmt.pix.sizeimage = static_cast<uint32_t>(cfg.width * cfg.height * 2);
    if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) != 0) {
        if (error) {
            *error = QStringLiteral("VIDIOC_S_FMT failed on %1: %2")
                          .arg(device, QString::fromLatin1(strerror(errno)));
        }
        close(m_fd);
        m_fd = -1;
        return false;
    }

    // Best effort: advertise the frame rate so consumers show something
    // sensible (v4l2loopback itself is not rate limited; our pacing thread
    // controls the actual rate).
    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parm.parm.output.timeperframe.numerator = 1;
    parm.parm.output.timeperframe.denominator = static_cast<uint32_t>(cfg.fps);
    ioctl(m_fd, VIDIOC_S_PARM, &parm);

    m_width = cfg.width;
    m_height = cfg.height;
    m_fps = cfg.fps;
    m_devicePath = device;
    return true;
}

void V4L2Writer::stop()
{
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

bool V4L2Writer::writeFrame(const uint8_t* data, int width, int height)
{
    if (m_fd < 0 || data == nullptr) {
        return false;
    }
    if (width != m_width || height != m_height) {
        return false; // needs a restart with a new configuration
    }
    const size_t size = static_cast<size_t>(width) * height * 2;
    ssize_t written = 0;
    while (written < static_cast<ssize_t>(size)) {
        const ssize_t n = ::write(m_fd, data + written, size - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        written += n;
    }
    return true;
}

QString V4L2Writer::describe() const
{
    return m_devicePath.isEmpty()
               ? QStringLiteral("v4l2loopback")
               : QStringLiteral("writing %1x%2 @ %3 fps to %4")
                     .arg(m_width)
                     .arg(m_height)
                     .arg(m_fps)
                     .arg(m_devicePath);
}

} // namespace vcam

#include "vcam/ImageSource.h"

#include <QImageReader>

namespace vcam {

ImageSource::ImageSource(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    m_base = reader.read();
    if (m_base.isNull()) {
        m_error = QObject::tr("Cannot load image %1: %2")
                      .arg(path, reader.errorString());
    }
}

bool ImageSource::latestFrame(int width, int height, QImage* out)
{
    if (m_base.isNull() || out == nullptr) {
        return false;
    }
    QMutexLocker lock(&m_mutex);
    if (m_scaledSize != QSize(width, height) || m_scaled.isNull()) {
        m_scaled = m_base.scaled(width, height, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_RGB888);
        m_scaledSize = QSize(width, height);
    }
    *out = m_scaled;
    return true;
}

} // namespace vcam

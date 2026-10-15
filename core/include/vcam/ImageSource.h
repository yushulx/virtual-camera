// ImageSource.h - a static image file, repeated at the target frame rate.

#pragma once

#include "vcam/MediaSource.h"

#include <QMutex>

namespace vcam {

class ImageSource final : public MediaSource
{
public:
    explicit ImageSource(const QString& path);

    bool latestFrame(int width, int height, QImage* out) override;
    QString lastError() const override { return m_error; }

private:
    QImage m_base;             // full resolution image
    QImage m_scaled;          // cached scaled copy (guarded by m_mutex)
    QSize m_scaledSize;
    QString m_error;
    mutable QMutex m_mutex;
};

} // namespace vcam

// ImageFit.h - fit a decoded frame into the camera's fixed canvas without
// changing its aspect ratio.
//
// The camera always delivers the configured width x height. A source with a
// different aspect ratio used to be stretched to fill that canvas, which
// turns a square QR code into a rectangle and distorts every barcode module
// the decoder measures. Letterboxing keeps the geometry of the original
// frame while still delivering exactly the resolution the consumers
// negotiated.

#pragma once

#include <QImage>
#include <QPainter>
#include <QRect>
#include <QSize>

namespace vcam {

inline QImage fitImageToCanvas(const QImage& source, int width, int height,
                               Qt::TransformationMode mode = Qt::SmoothTransformation,
                               QRgb background = qRgb(0, 0, 0))
{
    if (source.isNull() || width <= 0 || height <= 0) {
        return QImage();
    }
    if (source.size() == QSize(width, height)) {
        return source.convertToFormat(QImage::Format_RGB888);
    }

    QImage canvas(width, height, QImage::Format_RGB888);
    canvas.fill(background);

    const QSize fitted =
        source.size().scaled(QSize(width, height), Qt::KeepAspectRatio);
    const QRect target(QPoint((width - fitted.width()) / 2,
                              (height - fitted.height()) / 2),
                       fitted);

    QPainter painter(&canvas);
    if (mode == Qt::SmoothTransformation) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    painter.drawImage(target, source);
    painter.end();
    return canvas;
}

} // namespace vcam

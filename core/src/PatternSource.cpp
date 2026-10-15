#include "vcam/PatternSource.h"

#include <QDateTime>
#include <QPainter>

namespace vcam {

bool PatternSource::latestFrame(int width, int height, QImage* out)
{
    if (out == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    if (out->size() != QSize(width, height) || out->format() != QImage::Format_RGB32) {
        *out = QImage(width, height, QImage::Format_RGB32);
    }

    // SMPTE-style color bars + a moving marker + a frame counter.
    const QColor bars[8] = {
        QColor(192, 192, 192), QColor(192, 192, 0),  QColor(0, 192, 192),
        QColor(0, 192, 0),     QColor(192, 0, 192), QColor(192, 0, 0),
        QColor(0, 0, 192),     QColor(0, 0, 0),
    };
    QPainter p(out);
    const int barWidth = (width + 7) / 8;
    for (int i = 0; i < 8; ++i) {
        p.fillRect(i * barWidth, 0, barWidth, height * 3 / 4, bars[i]);
    }
    p.fillRect(0, height * 3 / 4, width, height / 4, QColor(16, 16, 16));

    // Moving marker: a white sweep so dropped frames are visible.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const int markerX = static_cast<int>((now / 10) % width);
    p.fillRect(markerX, 0, 8, height * 3 / 4, Qt::white);

    p.setPen(Qt::white);
    p.setFont(QFont(QString::fromLatin1("Arial"), height / 24));
    p.drawText(QRect(0, height * 3 / 4, width, height / 4),
               Qt::AlignCenter,
               QStringLiteral("Virtual Camera - test pattern"));
    p.end();
    return true;
}

} // namespace vcam

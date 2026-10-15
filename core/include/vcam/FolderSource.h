// FolderSource.h - a folder of images cycled as a slideshow: frame N shows
// image (N / imagesPerFrame) with the images sorted in natural order.

#pragma once

#include "vcam/MediaSource.h"

#include <QString>
#include <QStringList>
#include <vector>

namespace vcam {

class FolderSource final : public MediaSource
{
public:
    // Loads every image directly inside `folder` (natural-sorted). Returns
    // false when the folder is missing or contains no readable image.
    bool load(const QString& folder, QString* error);

    // Loads an explicit file list (multi-selected images), natural-sorted.
    bool loadFiles(const QStringList& files, QString* error);

    // Frames each image is held before advancing to the next one (>= 1).
    void setFramesPerImage(int frames) { m_framesPerImage = frames < 1 ? 1 : frames; }

    bool latestFrame(int width, int height, QImage* out) override;
    QString lastError() const override { return m_error; }

private:
    static bool naturalLessThan(const QString& a, const QString& b);

    QStringList m_files;
    std::vector<QImage> m_scaled;    // cache scaled copies (guarded)
    int m_scaledWidth = 0;
    int m_scaledHeight = 0;
    quint64 m_frameCount = 0;
    int m_framesPerImage = 60;       // ~2 s at 30 fps
    QString m_error;
};

} // namespace vcam

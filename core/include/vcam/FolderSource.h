// FolderSource.h - a folder of images cycled as a slideshow: frame N shows
// image (N / imagesPerFrame) with the images sorted in natural order.

#pragma once

#include "vcam/MediaSource.h"

#include <QHash>
#include <QMutex>
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
    // latestFrame is called from BOTH the camera pacing thread (stream
    // resolution) and the GUI preview timer (preview resolution). Keep one
    // cache per resolution - a single cache would be cleared on every call
    // as the two resolutions alternate - and guard everything with a
    // mutex: concurrent QImage cache mutation races the refcount and
    // crashes in Qt6Gui (0xC0000005).
    QMutex m_mutex;
    QHash<qint64, std::vector<QImage>> m_cacheBySize;
    quint64 m_frameCount = 0;
    int m_framesPerImage = 60;       // ~2 s at 30 fps
    QString m_error;
};

} // namespace vcam

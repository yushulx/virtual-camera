#include "vcam/FolderSource.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>

#include <algorithm>

namespace vcam {

namespace {

// Natural sort: image2.png before image10.png.
int naturalCompare(const QString& a, const QString& b)
{
    int ia = 0, ib = 0;
    while (ia < a.size() && ib < b.size()) {
        const QChar ca = a.at(ia);
        const QChar cb = b.at(ib);
        const bool da = ca.isDigit();
        const bool db = cb.isDigit();
        if (da && db) {
            // Compare the numeric runs.
            int ja = ia, jb = ib;
            while (ja < a.size() && a.at(ja).isDigit()) ++ja;
            while (jb < b.size() && b.at(jb).isDigit()) ++jb;
            const QStringView na(a.constData() + ia, ja - ia);
            const QStringView nb(b.constData() + ib, jb - ib);
            // Strip leading zeros and compare by length, then lexicographically.
            QStringView ta = na, tb = nb;
            while (ta.startsWith(u'0') && ta.size() > 1) ta = ta.mid(1);
            while (tb.startsWith(u'0') && tb.size() > 1) tb = tb.mid(1);
            if (ta.size() != tb.size()) {
                return ta.size() < tb.size() ? -1 : 1;
            }
            if (int cmp = ta.compare(tb) != 0) {
                return cmp;
            }
            ia = ja;
            ib = jb;
            continue;
        }
        if (ca != cb) {
            return ca.toLower() < cb.toLower() ? -1 : 1;
        }
        ++ia;
        ++ib;
    }
    return a.size() - b.size();
}

} // namespace

bool FolderSource::naturalLessThan(const QString& a, const QString& b)
{
    return naturalCompare(a, b) < 0;
}

bool FolderSource::load(const QString& folder, QString* error)
{
    QDir dir(folder);
    if (!dir.exists()) {
        if (error) {
            *error = QObject::tr("Folder does not exist: %1").arg(folder);
        }
        return false;
    }

    static const QStringList filters = {
        QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
        QStringLiteral("*.bmp"), QStringLiteral("*.webp"), QStringLiteral("*.gif"),
        QStringLiteral("*.tif"), QStringLiteral("*.tiff"),
    };
    QStringList files = dir.entryList(filters, QDir::Files | QDir::Readable,
                                      QDir::Unsorted);
    std::sort(files.begin(), files.end(), naturalLessThan);
    for (QString& file : files) {
        file = dir.filePath(file);
    }
    return loadFiles(files, error);
}

bool FolderSource::loadFiles(const QStringList& files, QString* error)
{
    m_files = files;
    std::sort(m_files.begin(), m_files.end(), naturalLessThan);
    if (m_files.isEmpty()) {
        if (error) {
            *error = QObject::tr("No image files to cycle through.");
        }
        return false;
    }
    QImageReader reader(m_files.first());
    reader.setAutoTransform(true);
    if (reader.read().isNull()) {
        if (error) {
            *error = QObject::tr("Cannot decode images: %1")
                         .arg(reader.errorString());
        }
        return false;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_cacheBySize.clear();
        m_frameCount = 0;
    }
    m_error.clear();
    return true;
}

bool FolderSource::latestFrame(int width, int height, QImage* out)
{
    if (m_files.isEmpty() || out == nullptr) {
        return false;
    }
    QMutexLocker lock(&m_mutex);
    const int imageCount = static_cast<int>(m_files.size());
    const int imageIndex = static_cast<int>(
        (m_frameCount / m_framesPerImage) % imageCount);
    ++m_frameCount;

    // Each (width, height) pair keeps its own decoded-and-scaled cache; the
    // pacing thread and the GUI preview use different sizes concurrently.
    const qint64 key = (static_cast<qint64>(width) << 32) | height;
    std::vector<QImage>& cache = m_cacheBySize[key];
    if (cache.size() != imageCount) {
        cache.assign(imageCount, QImage());
    }
    if (cache[imageIndex].isNull()) {
        QImageReader reader(m_files[imageIndex]);
        reader.setAutoTransform(true);
        QImage image = reader.read();
        if (image.isNull()) {
            // Skip unreadable files: show a dark placeholder instead.
            cache[imageIndex] = QImage(width, height, QImage::Format_RGB888);
            cache[imageIndex].fill(QColor(32, 32, 32));
        } else {
            cache[imageIndex] =
                image.scaled(width, height, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_RGB888);
        }
    }
    *out = cache[imageIndex];
    return true;
}

} // namespace vcam

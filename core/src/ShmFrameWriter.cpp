#include "vcam/ShmFrameWriter.h"

#include "vcam/SharedFrameProtocol.h"
#include "vcam/YuyvConverter.h"

#include <QDeadlineTimer>
#include <chrono>

#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

namespace vcam {

ShmFrameWriter::ShmFrameWriter()
{
#if !defined(Q_OS_WIN)
    m_handle = -1;
#endif
}

ShmFrameWriter::~ShmFrameWriter()
{
    stop();
}

#if defined(Q_OS_WIN)

bool ShmFrameWriter::mapRegion(QString* error)
{
    HANDLE mapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        kSharedRegionBytes,
        QString::fromLatin1(kShmWindowsName).toStdWString().c_str());
    if (mapping == nullptr) {
        if (error) {
            *error = QStringLiteral("CreateFileMapping failed: %1")
                         .arg(GetLastError());
        }
        return false;
    }
    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, kSharedRegionBytes);
    if (view == nullptr) {
        if (error) {
            *error = QStringLiteral("MapViewOfFile failed: %1").arg(GetLastError());
        }
        CloseHandle(mapping);
        return false;
    }
    m_handle = mapping;
    m_mapped = static_cast<uint8_t*>(view);
    return true;
}

void ShmFrameWriter::unmapRegion()
{
    if (m_mapped) {
        // Invalidate first so readers fall back to their test pattern.
        auto* header = reinterpret_cast<vcam::SharedFrameHeader*>(m_mapped);
        seqBegin(&header->frameSeq);
        header->magic = 0;
        seqEnd(&header->frameSeq);
        UnmapViewOfFile(m_mapped);
        m_mapped = nullptr;
    }
    if (m_handle) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
}

#else // macOS / POSIX

namespace {
// shm_open + ftruncate; returns -1 on failure (errno set).
int openShmRegion(const char* name, size_t bytes)
{
    const int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        return -1;
    }
    if (ftruncate(fd, static_cast<off_t>(bytes)) == 0) {
        return fd;
    }
    close(fd);
    return -1;
}
} // namespace

bool ShmFrameWriter::mapRegion(QString* error)
{
    int fd = openShmRegion(kShmPosixName, kSharedRegionBytes);
    if (fd < 0 && errno == EINVAL) {
        // A stale/corrupted object left by an earlier run can make ftruncate
        // fail (EINVAL). Unlink it and retry once with a fresh object; the
        // extension re-opens by name, so recreating it is always safe.
        // (Only for EINVAL: any other errno is not a stale-object symptom,
        // and unlinking an object another process may be using unguarded
        // would be destructive.)
        if (shm_unlink(kShmPosixName) == 0) {
            fd = openShmRegion(kShmPosixName, kSharedRegionBytes);
        }
    }
    if (fd < 0) {
        if (error) {
            *error = QStringLiteral("shm_open/ftruncate(%1) failed: %2")
                         .arg(QString::fromLatin1(kShmPosixName),
                              QString::fromLatin1(strerror(errno)));
        }
        return false;
    }
    void* view = mmap(nullptr, kSharedRegionBytes, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (view == MAP_FAILED) {
        if (error) {
            *error = QStringLiteral("mmap failed: %1")
                          .arg(QString::fromLatin1(strerror(errno)));
        }
        close(fd);
        return false;
    }
    // Keep the descriptor open: the mapping stays valid and the name stays
    // registered, so the extension can re-open the same object any time.
    m_handle = fd;
    m_mapped = static_cast<uint8_t*>(view);
    return true;
}

void ShmFrameWriter::unmapRegion()
{
    if (m_mapped) {
        auto* header = reinterpret_cast<vcam::SharedFrameHeader*>(m_mapped);
        seqBegin(&header->frameSeq);
        header->magic = 0;
        seqEnd(&header->frameSeq);
        munmap(m_mapped, kSharedRegionBytes);
        m_mapped = nullptr;
    }
    if (m_handle >= 0) {
        close(m_handle);
        m_handle = -1;
    }
}

#endif

bool ShmFrameWriter::start(const CameraConfig& cfg, QString* error)
{
    if (isRunning()) {
        return true;
    }
    if (!mapRegion(error)) {
        return false;
    }
    // Initialize the header. Readers validate magic/version before use, and
    // the seqlock makes the update atomic.
    auto* header = reinterpret_cast<vcam::SharedFrameHeader*>(m_mapped);
    memset(m_mapped, 0, kSharedHeaderSize);
    header->version = kSharedFrameVersion;
    header->width = static_cast<uint32_t>(cfg.width);
    header->height = static_cast<uint32_t>(cfg.height);
    header->stride = static_cast<uint32_t>(cfg.width * 2);
    header->format = kPixelFormatYuyv;
    header->frameSeq = 0; // even = stable
    m_framesWritten = 0;
    m_scratch.assign(static_cast<size_t>(cfg.width) * cfg.height * 2, 0);

    // Publish the header with a black payload. The region arrives zeroed, and
    // an all-zero YUYV frame is not black: it decodes to saturated green, so
    // consumers showed a green frame until the first video frame was decoded.
    const size_t frameBytes = static_cast<size_t>(cfg.width) * cfg.height * 2;
    fillYuyvBlack(m_mapped + kSharedHeaderSize, frameBytes);
    seqBegin(&header->frameSeq);
    header->magic = kSharedFrameMagic;
    seqEnd(&header->frameSeq);
    return true;
}

void ShmFrameWriter::stop()
{
    unmapRegion();
}

bool ShmFrameWriter::writeFrame(const uint8_t* data, int width, int height)
{
    if (data == nullptr || m_mapped == nullptr) {
        return false;
    }
    if (width <= 0 || height <= 0 ||
        static_cast<size_t>(width) * height * 2 > kMaxFrameBytes ||
        static_cast<size_t>(width) * height * 2 > m_scratch.size()) {
        return false;
    }
    auto* header = reinterpret_cast<vcam::SharedFrameHeader*>(m_mapped);
    const uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    seqBegin(&header->frameSeq);
    header->width = static_cast<uint32_t>(width);
    header->height = static_cast<uint32_t>(height);
    header->stride = static_cast<uint32_t>(width * 2);
    header->format = kPixelFormatYuyv;
    header->frameCount = m_framesWritten;
    header->timestampNs = now;
    memcpy(m_mapped + kSharedHeaderSize, data,
           static_cast<size_t>(width) * height * 2);
    seqEnd(&header->frameSeq);

    ++m_framesWritten;
    return true;
}

QString ShmFrameWriter::describe() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("feeding the DirectShow filter via shared memory");
#else
    return QStringLiteral("feeding the camera extension via shared memory");
#endif
}

} // namespace vcam

#include "SharedFrameReader.h"

namespace vcamdriver {

SharedFrameReader::~SharedFrameReader()
{
    closeMapping();
}

void SharedFrameReader::closeMapping()
{
    if (m_view) {
        UnmapViewOfFile(m_view);
        m_view = nullptr;
    }
    if (m_mapping) {
        CloseHandle(m_mapping);
        m_mapping = nullptr;
    }
}

bool SharedFrameReader::ensureMapped()
{
    if (m_view) {
        return true;
    }
    // The writer creates the section; when it is missing the camera is not
    // running. Re-opened lazily so a later controller start is picked up.
    // The shared protocol carries the name as ASCII; widen it (the build is
    // UNICODE and the name is plain ASCII).
    wchar_t name[64] = {};
    for (size_t i = 0; i < sizeof(vcam::kShmWindowsName) - 1 && i < 63; ++i) {
        name[i] = static_cast<wchar_t>(
            static_cast<unsigned char>(vcam::kShmWindowsName[i]));
    }
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
    if (mapping == nullptr) {
        return false;
    }
    uint8_t* view = static_cast<uint8_t*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, vcam::kSharedRegionBytes));
    if (view == nullptr) {
        CloseHandle(mapping);
        return false;
    }
    m_mapping = mapping;
    m_view = view;
    return true;
}

bool SharedFrameReader::readLatest(uint8_t* dst, size_t dstBytes,
                                    uint32_t* width, uint32_t* height,
                                    uint32_t* stride)
{
    if (dst == nullptr || dstBytes < vcam::kMaxFrameBytes) {
        return false;
    }
    if (!ensureMapped()) {
        return false;
    }
    // Seqlock read: check sequence, copy, re-check. The atomic loads act as
    // full fences, so the copy cannot be reordered across them.
    for (int attempt = 0; attempt < 4; ++attempt) {
        const auto* live =
            reinterpret_cast<const vcam::SharedFrameHeader*>(m_view);
        if (live->magic != vcam::kSharedFrameMagic ||
            live->version != vcam::kSharedFrameVersion) {
            closeMapping(); // the controller recreated the region
            if (!ensureMapped()) {
                return false;
            }
            continue;
        }
        const uint32_t seq1 = vcam::seqLoadAcquire(&live->frameSeq);
        if (seq1 & 1u) {
            Sleep(0); // the writer is in the critical section
            continue;
        }
        const uint32_t w = live->width;
        const uint32_t h = live->height;
        const uint32_t s = live->stride;
        if (w == 0 || h == 0 || s != w * 2u ||
            static_cast<size_t>(s) * h > dstBytes) {
            return false;
        }
        memcpy(dst, m_view + vcam::kSharedHeaderSize,
               static_cast<size_t>(s) * h);
        const uint32_t seq2 = vcam::seqLoadAcquire(&live->frameSeq);
        if (seq1 != seq2) {
            continue; // torn read: retry
        }
        *width = w;
        *height = h;
        *stride = s;
        return true;
    }
    return false;
}

bool SharedFrameReader::readHeader(uint32_t* width, uint32_t* height,
                                   uint32_t* fpsHint)
{
    if (!ensureMapped()) {
        return false;
    }
    const auto* live =
        reinterpret_cast<const vcam::SharedFrameHeader*>(m_view);
    if (live->magic != vcam::kSharedFrameMagic ||
        live->version != vcam::kSharedFrameVersion) {
        closeMapping();
        return false;
    }
    *width = live->width;
    *height = live->height;
    // The protocol does not carry the fps (it only paces the writer); the
    // drivers advertise a sensible default.
    *fpsHint = vcam::kDefaultFps;
    return *width > 0 && *height > 0;
}

} // namespace vcamdriver

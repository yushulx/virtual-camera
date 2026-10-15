// SharedFrameReader.h - reader side of the shared memory frame region
// (written by VirtualCamera.exe, see core/src/ShmFrameWriter.cpp).

#pragma once

#include "Common.h"

namespace vcamdriver {

class SharedFrameReader
{
public:
    SharedFrameReader() = default;
    ~SharedFrameReader();

    // Copy the latest frame into dst. Returns false when the controller is
    // not running (no region) - the caller then falls back to the test
    // pattern. `dst` must have room for kMaxFrameBytes.
    bool readLatest(uint8_t* dst, size_t dstBytes, uint32_t* width,
                    uint32_t* height, uint32_t* stride);

    // Header only (used to advertise the current format even when the
    // controller is not streaming yet).
    bool readHeader(uint32_t* width, uint32_t* height, uint32_t* fpsHint);

private:
    bool ensureMapped();
    void closeMapping();

    HANDLE m_mapping = nullptr;
    uint8_t* m_view = nullptr;
};

} // namespace vcamdriver

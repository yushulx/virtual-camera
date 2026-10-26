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
    // not running (no region) or the copy could not be completed; the caller
    // keeps showing its previous frame. `dst` must have room for the current
    // frame (stride * height). frameCount receives the writer's frame counter
    // so the caller can skip reconverting a frame it already has.
    bool readLatest(uint8_t* dst, size_t dstBytes, uint32_t* width,
                    uint32_t* height, uint32_t* stride,
                    uint32_t* frameCount = nullptr);

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

// ShmFrameWriter.h - writer side of the shared memory frame region
// (Windows and macOS; the DirectShow filter and the CMIO Camera Extension
// read it on the other side). See vcam/SharedFrameProtocol.h.

#pragma once

#include "vcam/FrameWriter.h"

#include <cstdint>
#include <vector>

#if defined(Q_OS_WIN)
using ShmNativeHandle = void*; // HANDLE
#else
using ShmNativeHandle = int;  // file descriptor
#endif

namespace vcam {

class ShmFrameWriter final : public FrameWriter
{
public:
    ShmFrameWriter();
    ~ShmFrameWriter() override;

    bool start(const CameraConfig& cfg, QString* error) override;
    void stop() override;
    bool writeFrame(const uint8_t* data, int width, int height) override;
    bool isRunning() const override { return m_mapped != nullptr; }
    QString describe() const override;

private:
    bool mapRegion(QString* error);
    void unmapRegion();

    uint8_t* m_mapped = nullptr;  // header + payload
    ShmNativeHandle m_handle{};

    // Scratch used to build the full region image of a frame before the
    // seqlock critical section.
    std::vector<uint8_t> m_scratch;

    uint32_t m_framesWritten = 0;
};

} // namespace vcam

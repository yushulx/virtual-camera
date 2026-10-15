// FrameWriter.h - platform side of the camera output.

#pragma once

#include "vcam/Config.h"

#include <QString>
#include <cstdint>

namespace vcam {

class FrameWriter
{
public:
    virtual ~FrameWriter() = default;

    // Prepare the output for the given configuration. Returns false and
    // fills `error` when the virtual camera is not available.
    virtual bool start(const CameraConfig& cfg, QString* error) = 0;

    virtual void stop() = 0;

    // Write one YUYV frame (width * height * 2 bytes). Called from the
    // pacing thread at roughly cfg.fps calls per second.
    virtual bool writeFrame(const uint8_t* data, int width, int height) = 0;

    virtual bool isRunning() const = 0;

    // Human readable description of the output (shown in the GUI status).
    virtual QString describe() const = 0;
};

} // namespace vcam

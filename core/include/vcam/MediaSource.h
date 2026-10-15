// MediaSource.h - common interface for everything that can produce frames.

#pragma once

#include <QImage>
#include <QString>

namespace vcam {

class MediaSource
{
public:
    virtual ~MediaSource() = default;

    // Return the latest frame scaled to width x height. Returns false when
    // no frame is available yet. Called from the controller's pacing thread;
    // implementations must be safe to call from a non-GUI thread.
    virtual bool latestFrame(int width, int height, QImage* out) = 0;

    virtual QString lastError() const = 0;
};

} // namespace vcam

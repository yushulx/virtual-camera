// PatternSource.h - built-in moving color test pattern (no file needed).

#pragma once

#include "vcam/MediaSource.h"

namespace vcam {

class PatternSource final : public MediaSource
{
public:
    // `clockMs` drives the moving marker so every frame differs (proves the
    // camera is alive, like a real camera would).
    bool latestFrame(int width, int height, QImage* out) override;
    QString lastError() const override { return QString(); }
};

} // namespace vcam

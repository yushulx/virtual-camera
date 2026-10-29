// YuyvConverter.h - RGB (QImage) <-> YUYV (a.k.a. YUY2) conversion.

#pragma once

#include <QImage>
#include <cstddef>
#include <cstdint>

namespace vcam {

// Convert an RGB(A) image to a packed YUYV frame (BT.601 limited range,
// 2 bytes per pixel, 4:2:2 with U/V shared between pixel pairs).
//
// The image is converted to RGB888 layout internally (any QImage format that
// toFormat can convert is accepted). `dst` must provide width * height * 2
// bytes; returns false when image or capacity is invalid.
bool convertImageToYuyv(const QImage& image, uint8_t* dst, size_t dstCapacity);

// Inverse conversion (used by the GUI preview and the unit tests).
QImage convertYuyvToImage(const uint8_t* data, int width, int height, size_t capacity);

// Fill a YUYV buffer with limited-range black (Y=16, U=V=128).
//
// Both writers publish this before the first real frame: an all-zero buffer
// is not black in YUV, it decodes to saturated green (Y=0, U=V=0), which is
// what consumers showed while a video source was still opening its decoder.
void fillYuyvBlack(uint8_t* data, size_t bytes);

} // namespace vcam

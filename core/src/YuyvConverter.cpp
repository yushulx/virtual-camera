#include "vcam/YuyvConverter.h"

#include <QVector>

namespace vcam {
namespace {

// BT.601 limited range ("studio swing"): Y in [16, 235], U/V in [16, 240].
inline uint8_t rgbToY(int r, int g, int b)
{
    // y = (66*r + 129*g + 25*b + 128) >> 8 + 16
    return static_cast<uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
}

inline void rgbToUV(int r, int g, int b, uint8_t* u, uint8_t* v)
{
    // u = (-38*r - 74*g + 112*b + 128) >> 8 + 128
    // v = (112*r - 94*g - 18*b + 128) >> 8 + 128
    *u = static_cast<uint8_t>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
    *v = static_cast<uint8_t>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
}

} // namespace

bool convertImageToYuyv(const QImage& image, uint8_t* dst, size_t dstCapacity)
{
    if (image.isNull() || dst == nullptr) {
        return false;
    }
    const int width = image.width();
    const int height = image.height();
    const size_t needed = static_cast<size_t>(width) * height * 2;
    if (dstCapacity < needed) {
        return false;
    }

    // Work from a well-defined byte layout.
    const QImage rgb = image.format() == QImage::Format_RGB888
                           ? image
                           : image.convertToFormat(QImage::Format_RGB888);
    if (rgb.isNull() || rgb.format() != QImage::Format_RGB888) {
        return false;
    }

    for (int y = 0; y < height; ++y) {
        const uint8_t* src = rgb.constScanLine(y); // width * 3 bytes, 32-bit aligned
        uint8_t* out = dst + static_cast<size_t>(y) * width * 2;
        for (int x = 0; x < width; x += 2) {
            const uint8_t r0 = src[x * 3 + 0];
            const uint8_t g0 = src[x * 3 + 1];
            const uint8_t b0 = src[x * 3 + 2];
            const int x1 = x + 1 < width ? x + 1 : x; // last odd pixel repeats
            const uint8_t r1 = src[x1 * 3 + 0];
            const uint8_t g1 = src[x1 * 3 + 1];
            const uint8_t b1 = src[x1 * 3 + 2];

            out[0] = rgbToY(r0, g0, b0);                    // Y0
            uint8_t u, v;
            rgbToUV((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, &u, &v);
            out[1] = u;                                     // U (shared)
            out[2] = rgbToY(r1, g1, b1);                    // Y1
            out[3] = v;                                     // V (shared)
            out += 4;
        }
    }
    return true;
}

QImage convertYuyvToImage(const uint8_t* data, int width, int height, size_t capacity)
{
    if (data == nullptr || width <= 0 || height <= 0) {
        return QImage();
    }
    if (capacity < static_cast<size_t>(width) * height * 2) {
        return QImage();
    }
    QImage image(width, height, QImage::Format_RGB888);
    const auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
    for (int y = 0; y < height; ++y) {
        const uint8_t* in = data + static_cast<size_t>(y) * width * 2;
        uint8_t* out = image.scanLine(y);
        for (int x = 0; x < width; x += 2) {
            const int y0 = in[0], u = in[1] - 128, y1 = in[2], v = in[3] - 128;
            for (int k = 0; k < 2 && x + k < width; ++k) {
                const int yy = k == 0 ? y0 : y1;
                const int r = yy + ((91881 * v) >> 16);
                const int g = yy - ((22554 * u + 46802 * v) >> 16);
                const int b = yy + ((116130 * u) >> 16);
                out[(x + k) * 3 + 0] = static_cast<uint8_t>(clamp(r));
                out[(x + k) * 3 + 1] = static_cast<uint8_t>(clamp(g));
                out[(x + k) * 3 + 2] = static_cast<uint8_t>(clamp(b));
            }
            in += 4;
        }
    }
    return image;
}

} // namespace vcam

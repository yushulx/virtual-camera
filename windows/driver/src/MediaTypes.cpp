#include "MediaTypes.h"

// WIN32_LEAN_AND_MEAN excludes mmreg.h where MAKEFOURCC normally lives.
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                    \
    ((static_cast<DWORD>(static_cast<BYTE>(ch0))) |                       \
     (static_cast<DWORD>(static_cast<BYTE>(ch1)) << 8) |                  \
     (static_cast<DWORD>(static_cast<BYTE>(ch2)) << 16) |                 \
     (static_cast<DWORD>(static_cast<BYTE>(ch3)) << 24))
#endif

namespace vcamdriver {

bool makeMediaType(const CameraFormat& fmt, AM_MEDIA_TYPE* mt)
{
    if (mt == nullptr || fmt.width == 0 || fmt.height == 0 || fmt.fps == 0) {
        return false;
    }
    VIDEOINFOHEADER* vih = static_cast<VIDEOINFOHEADER*>(
        CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
    if (vih == nullptr) {
        return false;
    }
    ZeroMemory(vih, sizeof(VIDEOINFOHEADER));
    vih->rcSource.right = static_cast<LONG>(fmt.width);
    vih->rcSource.bottom = static_cast<LONG>(fmt.height);
    vih->rcTarget = vih->rcSource;
    vih->AvgTimePerFrame =
        static_cast<REFERENCE_TIME>(10000000i64 / fmt.fps);
    vih->dwBitRate = fmt.width * fmt.height * 2 * 8 * fmt.fps;
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth = static_cast<LONG>(fmt.width);
    vih->bmiHeader.biHeight = static_cast<LONG>(fmt.height);
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 16;
    vih->bmiHeader.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
    vih->bmiHeader.biSizeImage = fmt.width * fmt.height * 2;

    ZeroMemory(mt, sizeof(AM_MEDIA_TYPE));
    mt->majortype = MEDIATYPE_Video;
    mt->subtype = MEDIASUBTYPE_YUY2;
    mt->formattype = FORMAT_VideoInfo;
    mt->bTemporalCompression = FALSE;
    mt->lSampleSize = fmt.width * fmt.height * 2;
    mt->cbFormat = sizeof(VIDEOINFOHEADER);
    mt->pbFormat = reinterpret_cast<BYTE*>(vih);
    return true;
}

void freeMediaType(AM_MEDIA_TYPE* mt)
{
    if (mt == nullptr) {
        return;
    }
    if (mt->pbFormat) {
        CoTaskMemFree(mt->pbFormat);
        mt->pbFormat = nullptr;
    }
    if (mt->pUnk) {
        mt->pUnk->Release();
        mt->pUnk = nullptr;
    }
}

bool copyMediaType(AM_MEDIA_TYPE* dst, const AM_MEDIA_TYPE* src)
{
    if (dst == nullptr || src == nullptr) {
        return false;
    }
    *dst = *src;
    if (src->pUnk) {
        src->pUnk->AddRef();
    }
    if (src->cbFormat && src->pbFormat) {
        dst->pbFormat = static_cast<BYTE*>(CoTaskMemAlloc(src->cbFormat));
        if (dst->pbFormat == nullptr) {
            dst->cbFormat = 0;
            return false;
        }
        memcpy(dst->pbFormat, src->pbFormat, src->cbFormat);
    } else {
        dst->pbFormat = nullptr;
        dst->cbFormat = 0;
    }
    return true;
}

bool formatFromMediaType(const AM_MEDIA_TYPE* mt, CameraFormat* out)
{
    if (mt == nullptr || out == nullptr) {
        return false;
    }
    if (mt->majortype != MEDIATYPE_Video || mt->subtype != MEDIASUBTYPE_YUY2 ||
        mt->formattype != FORMAT_VideoInfo ||
        mt->cbFormat < sizeof(VIDEOINFOHEADER) || mt->pbFormat == nullptr) {
        return false;
    }
    const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(mt->pbFormat);
    if (vih->bmiHeader.biWidth <= 0 || vih->bmiHeader.biHeight <= 0) {
        return false;
    }
    out->width = static_cast<uint32_t>(vih->bmiHeader.biWidth);
    out->height = static_cast<uint32_t>(vih->bmiHeader.biHeight);
    out->fps = vih->AvgTimePerFrame > 0
                   ? static_cast<uint32_t>(10000000i64 / vih->AvgTimePerFrame)
                   : vcam::kDefaultFps;
    return true;
}

bool mediaTypesEqual(const AM_MEDIA_TYPE* a, const AM_MEDIA_TYPE* b)
{
    if (a == b) {
        return true;
    }
    if (a == nullptr || b == nullptr) {
        return false;
    }
    if (a->majortype != b->majortype || a->subtype != b->subtype ||
        a->formattype != b->formattype || a->cbFormat != b->cbFormat) {
        return false;
    }
    if (a->cbFormat == 0) {
        return true;
    }
    if (a->pbFormat == nullptr || b->pbFormat == nullptr) {
        return false;
    }
    return memcmp(a->pbFormat, b->pbFormat, a->cbFormat) == 0;
}

} // namespace vcamdriver

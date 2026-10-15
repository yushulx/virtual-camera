// MediaTypes.h - AM_MEDIA_TYPE helpers for the single YUYV capture format.

#pragma once

#include "Common.h"

namespace vcamdriver {

struct CameraFormat
{
    uint32_t width = vcam::kDefaultWidth;
    uint32_t height = vcam::kDefaultHeight;
    uint32_t fps = vcam::kDefaultFps;
};

// Fill `mt` with a VIDEOINFOHEADER-based YUYV media type for `fmt`.
// Allocates mt->pbFormat with CoTaskMemAlloc; caller owns it.
bool makeMediaType(const CameraFormat& fmt, AM_MEDIA_TYPE* mt);

// Free a media type allocated by makeMediaType / copyMediaType.
void freeMediaType(AM_MEDIA_TYPE* mt);

// Deep copy (CoTaskMemAlloc for pbFormat); returns false when out of memory.
bool copyMediaType(AM_MEDIA_TYPE* dst, const AM_MEDIA_TYPE* src);

// Extract the format from a media type; false when it is not our format.
bool formatFromMediaType(const AM_MEDIA_TYPE* mt, CameraFormat* out);

// Exact comparison ignoring pbFormat padding.
bool mediaTypesEqual(const AM_MEDIA_TYPE* a, const AM_MEDIA_TYPE* b);

} // namespace vcamdriver

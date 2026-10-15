// Common.h - shared includes and GUID declarations for the DirectShow
// capture filter.
//
// GUID policy: this header DECLARES every GUID (DEFINE_GUID without INITGUID
// expands to an extern declaration). Exactly one translation unit,
// guids.cpp, includes <initguid.h> first, which turns the same DEFINE_GUID
// lines into definitions. This is the standard DirectShow pattern.

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#if defined(_MSC_VER)
// The UCRT wide-string fast paths use SSE intrinsics on x64: make sure they
// are declared before any system header pulls in <wchar.h>.
#include <intrin.h>
#endif

#include <windows.h>
#include <dshow.h>   // strmif.h + uuids.h (GUID declarations) + evcode.h + ...
#include <vfwmsgs.h> // VFW_E_* status codes

#include "vcam/SharedFrameProtocol.h"

#include <cstdint>
#include <cstring>

// {D4A1F5B2-6E38-4C7A-9B2E-1F8D3A5C7E64}
DEFINE_GUID(CLSID_VirtualCameraFilter, 0xd4a1f5b2, 0x6e38, 0x4c7a, 0x9b, 0x2e,
            0x1f, 0x8d, 0x3a, 0x5c, 0x7e, 0x64);

// {860BB310-5D01-11D0-BD3B-00A0C911CE86}
// DirectShow "Video Input Devices" category (CLSID_VideoInputDeviceCategory).
DEFINE_GUID(VCAM_CLSID_VideoInputDeviceCategory, 0x860bb310, 0x5d01, 0x11d0,
            0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);

// {FB6C4281-0353-11D1-905F-0000C0CC16BA}  PIN_CATEGORY_CAPTURE.
DEFINE_GUID(VCAM_PIN_CATEGORY_CAPTURE, 0xfb6c4281, 0x353, 0x11d1, 0x90, 0x5f,
            0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);

namespace vcamdriver {

extern HMODULE g_module;

// The friendly name under which the camera shows up.
inline constexpr wchar_t kCameraName[] = L"Virtual Camera";

// Debug logging via OutputDebugString (free in release builds).
#if defined(_DEBUG)
void trace(const wchar_t* fmt, ...);
#define VCAM_TRACE(...) vcamdriver::trace(__VA_ARGS__)
#else
#define VCAM_TRACE(...) ((void)0)
#endif

} // namespace vcamdriver

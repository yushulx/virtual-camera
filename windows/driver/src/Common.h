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
#include <ks.h>
#include <ksmedia.h> // AMPROPSETID_Pin / AMPROPERTY_PIN_CATEGORY (IKsPropertySet)

#include "vcam/SharedFrameProtocol.h"

#include <cstdint>
#include <cstring>

// {D4A1F5B2-6E38-4C7A-9B2E-1F8D3A5C7E64}
DEFINE_GUID(CLSID_VirtualCameraFilter, 0xd4a1f5b2, 0x6e38, 0x4c7a, 0x9b, 0x2e,
            0x1f, 0x8d, 0x3a, 0x5c, 0x7e, 0x64);

namespace vcamdriver {

// The friendly name under which the camera shows up.
inline constexpr wchar_t kCameraName[] = L"Virtual Camera";

#if defined(VCAM_DRIVER_TRACE)
// Local-diagnosis trace: appends to a fixed file (unbuffered flush per line).
void traceLine(const char* text);
#define VCAM_TRACE_P(text) vcamdriver::traceLine(text)
#else
#define VCAM_TRACE_P(text) ((void)0)
#endif

// Debug logging via OutputDebugString (free in release builds).
#if defined(_DEBUG)
void trace(const wchar_t* fmt, ...);
#define VCAM_TRACE(...) vcamdriver::trace(__VA_ARGS__)
#else
#define VCAM_TRACE(...) ((void)0)
#endif

} // namespace vcamdriver

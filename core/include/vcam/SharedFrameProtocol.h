// SharedFrameProtocol.h - the wire format between the controller (the GUI or
// CLI process that produces frames) and the virtual camera drivers (the
// DirectShow filter on Windows, the CMIO Camera Extension on macOS).
//
// It is deliberately free of Qt and of any platform GUI framework so it can
// be included from:
//   - core (the writer side, C++17 / Qt project)
//   - windows/driver (the reader side, plain C++ / Windows SDK)
//   - macos/extension (the reader side; the same layout is re-declared in
//     Swift, see macos/extension/Sources/SharedFrameStore.swift)
//
// The protocol is a single-header lock-free "latest frame" buffer guarded by
// a seqlock: the writer bumps a sequence counter to an odd value, copies the
// header + payload, and bumps it again to an even value. Readers retry while
// the counter is odd or changed during the copy. This gives tear-free frame
// updates without any lock that could deadlock inside a consumer process.

#pragma once

#include <stdint.h>

namespace vcam {

// Region naming:
//   Windows: CreateFileMapping name  "Local\\VirtualCameraFrame"
//   macOS:   shm_open name           "virtualcamera_frame"
inline constexpr char kShmWindowsName[] = "Local\\VirtualCameraFrame";
inline constexpr char kShmPosixName[] = "virtualcamera_frame";

// 'VCAM' little endian.
inline constexpr uint32_t kSharedFrameMagic = 0x4D414356u;
inline constexpr uint32_t kSharedFrameVersion = 1;

// Pixel formats (header.format).
inline constexpr uint32_t kPixelFormatYuyv = 1;

// Fixed region layout: [header][payload]. The region always has room for
// the largest supported frame, so changing the resolution never requires
// re-allocating (and consumers never need to re-map).
inline constexpr uint32_t kSharedHeaderSize = 64;
inline constexpr uint32_t kMaxFrameWidth = 1920;
inline constexpr uint32_t kMaxFrameHeight = 1080;
inline constexpr uint32_t kMaxFrameBytes = kMaxFrameWidth * kMaxFrameHeight * 2; // YUYV
inline constexpr uint32_t kSharedRegionBytes = kSharedHeaderSize + kMaxFrameBytes;

// Fallback format used by the drivers when the controller has never run.
inline constexpr uint32_t kDefaultWidth = 1280;
inline constexpr uint32_t kDefaultHeight = 720;
inline constexpr uint32_t kDefaultFps = 30;

#pragma pack(push, 8)
struct SharedFrameHeader {
    uint32_t magic;        // kSharedFrameMagic while the region is initialized
    uint32_t version;      // kSharedFrameVersion
    uint32_t width;        // pixels
    uint32_t height;       // pixels
    uint32_t stride;       // bytes per row (width * 2 for YUYV)
    uint32_t format;       // kPixelFormatYuyv
    uint32_t reserved0;    // must be 0
    uint32_t reserved1;    // must be 0
    uint32_t frameSeq;     // seqlock: even = stable, odd = being written
    uint32_t frameCount;   // frames written modulo 2^32 (informational)
    uint64_t timestampNs;  // writer's steady clock (informational)
};
#pragma pack(pop)

static_assert(sizeof(SharedFrameHeader) <= kSharedHeaderSize,
              "SharedFrameHeader must stay within kSharedHeaderSize so the "
              "Swift re-declaration keeps working");
static_assert(kMaxFrameBytes >= kDefaultWidth * kDefaultHeight * 2,
              "the default frame must fit into the shared region");

// --- atomics usable on memory shared between processes ---------------------

// MSVC: Interlocked* are full fences and work on pages mapped into multiple
// processes. GCC/Clang: __atomic builtins give the same guarantees.

inline uint32_t seqLoadAcquire(const volatile uint32_t* seq) {
#if defined(_MSC_VER)
    return static_cast<uint32_t>(
        InterlockedCompareExchange(reinterpret_cast<volatile long*>(const_cast<uint32_t*>(seq)), 0, 0));
#else
    return __atomic_load_n(seq, __ATOMIC_ACQUIRE);
#endif
}

// Writer: enter the critical section (sequence becomes odd).
inline void seqBegin(volatile uint32_t* seq) {
#if defined(_MSC_VER)
    InterlockedIncrement(reinterpret_cast<volatile long*>(seq));
#else
    __atomic_fetch_add(seq, 1u, __ATOMIC_ACQ_REL);
#endif
}

// Writer: leave the critical section (sequence becomes even again); all
// payload writes happen-before this store.
inline void seqEnd(volatile uint32_t* seq) {
#if defined(_MSC_VER)
    InterlockedIncrement(reinterpret_cast<volatile long*>(seq));
#else
    __atomic_fetch_add(seq, 1u, __ATOMIC_RELEASE);
#endif
}

} // namespace vcam

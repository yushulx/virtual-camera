// shm-dump.cpp - standalone check of SharedFrameReader against the running
// controller: reads two frames from the shared memory region and writes
// them as BMP files. No DirectShow involved.

#include "SharedFrameReader.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <vector>

static void printLine(const char* text)
{
    FILE* f = nullptr;
    fopen_s(&f, "D:/code/virtual-camera/build/vcam-crash.log", "a");
    if (f != nullptr) {
        fprintf(f, "%s\n", text);
        fclose(f);
    }
    printf("%s\n", text);
}

LONG WINAPI vcamVectoredHandler(EXCEPTION_POINTERS* ep)
{
    char msg[160];
    sprintf_s(msg,
              "EXCEPTION code=0x%08lX addr=%p op=%s target=%p",
              ep->ExceptionRecord->ExceptionCode,
              ep->ExceptionRecord->ExceptionAddress,
              ep->ExceptionRecord->ExceptionInformation[0] == 0 ? "READ"
                  : (ep->ExceptionRecord->ExceptionInformation[0] == 1 ? "WRITE"
                                                                       : "EXEC"),
              (void*)ep->ExceptionRecord->ExceptionInformation[1]);
    printLine(msg);
    return EXCEPTION_CONTINUE_SEARCH;
}

#if defined(VCAM_DRIVER_TRACE)
namespace vcamdriver {
void traceLine(const char* text)
{
    FILE* f = nullptr;
    fopen_s(&f, "D:/code/virtual-camera/build/vcam-trace.log", "a");
    if (f != nullptr) {
        fprintf(f, "[%lu] %s\n", GetCurrentThreadId(), text);
        fclose(f);
    }
}
} // namespace vcamdriver
#endif
#include <cstdio>

static void writeBmp(const char* path, const uint8_t* yuyv, int width,
                     int height)
{
    const int stride = (width * 3 + 3) & ~3;
    std::vector<uint8_t> rgb(static_cast<size_t>(stride) * height, 0);
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = yuyv + static_cast<size_t>(y) * width * 2;
        uint8_t* dst = rgb.data() + static_cast<size_t>(height - 1 - y) * stride;
        for (int x = 0; x < width; x += 2) {
            const int y0 = src[0], u = src[1] - 128, y1 = src[2],
                      v = src[3] - 128;
            for (int k = 0; k < 2 && x + k < width; ++k) {
                const int yy = k == 0 ? y0 : y1;
                const int r = yy + ((91881 * v) >> 16);
                const int g = yy - ((22554 * u + 46802 * v) >> 16);
                const int b = yy + ((116130 * u) >> 16);
                uint8_t* pixel = dst + (x + k) * 3;
                pixel[0] = static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
                pixel[1] = static_cast<uint8_t>(g < 0 ? 0 : (g > 255 ? 255 : g));
                pixel[2] = static_cast<uint8_t>(b < 0 ? 0 : (b > 255 ? 255 : b));
            }
            src += 4;
        }
    }
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + static_cast<DWORD>(rgb.size());
    ih.biSize = sizeof(ih);
    ih.biWidth = width;
    ih.biHeight = height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = static_cast<DWORD>(rgb.size());
    FILE* f = nullptr;
    fopen_s(&f, path, "wb");
    if (f) {
        fwrite(&fh, sizeof(fh), 1, f);
        fwrite(&ih, sizeof(ih), 1, f);
        fwrite(rgb.data(), 1, rgb.size(), f);
        fclose(f);
        printf("wrote %s\n", path);
    }
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(1, vcamVectoredHandler);
    vcamdriver::SharedFrameReader reader;
    uint32_t w = 0, h = 0, fps = 0;
    if (!reader.readHeader(&w, &h, &fps)) {
        printf("readHeader FAILED\n");
        return 1;
    }
    printf("header: %ux%u @%ufps\n", w, h, fps);

    std::vector<uint8_t> buffer(static_cast<size_t>(w) * h * 2);
    uint32_t sw = 0, sh = 0, sstride = 0;
    if (!reader.readLatest(buffer.data(), buffer.size(), &sw, &sh, &sstride)) {
        printf("readLatest #1 FAILED\n");
        return 1;
    }
    printf("frame1: %ux%u stride=%u\n", sw, sh, sstride);
    writeBmp("D:/code/virtual-camera/build/shm1.bmp", buffer.data(), w, h);

    Sleep(2000);
    if (!reader.readLatest(buffer.data(), buffer.size(), &sw, &sh, &sstride)) {
        printf("readLatest #2 FAILED\n");
        return 1;
    }
    printf("frame2: %ux%u stride=%u\n", sw, sh, sstride);
    writeBmp("D:/code/virtual-camera/build/shm2.bmp", buffer.data(), w, h);
    return 0;
}

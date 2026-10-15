// dllmain.cpp - DLL entry points and registration, mirroring the proven
// softcam layout:
//   * dllentry.cpp (base classes) provides DllGetClassObject /
//     DllCanUnloadNow / DllEntryPoint driven by g_Templates
//   * this file provides DllMain, DllRegisterServer / DllUnregisterServer
//     (AMovieDllRegisterServer2 + IFilterMapper2 in the Video Input Device
//     category) and the single TU that instantiates the GUIDs

// GUIDs are instantiated once in guids.cpp (uuids.h has no include guard,
// so initguid + two dshow.h pulls in one TU would redefine everything).
#include "VirtualCamera.h" // pulls <streams.h> before the Windows headers

#include <cstdio>

namespace {

const wchar_t kFilterName[] = L"Virtual Camera";

// Pin/type description used for the filter registration (FilterData).
const AMOVIESETUP_MEDIATYPE kPinTypes[] = {
    {&MEDIATYPE_Video, &MEDIASUBTYPE_NULL},
};

const AMOVIESETUP_PIN kPins[] = {
    {const_cast<LPWSTR>(L"Output"), // pin name
     FALSE,                         // rendered
     TRUE,                          // output
     FALSE,                         // can have none
     FALSE,                         // can have many
     &CLSID_NULL,                   // connects to filter
     nullptr,                       // connects to pin
     1,                             // media type count
     kPinTypes},
};

const REGFILTER2 kRegFilter2 = {1, MERIT_DO_NOT_USE, 1, kPins};

CUnknown* WINAPI CreateFilterInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    return vcamdriver::VirtualCamera::CreateInstance(lpunk, phr);
}

} // namespace

// COM global table of objects in this DLL (consumed by dllentry.cpp).
CFactoryTemplate g_Templates[] = {
    {const_cast<LPWSTR>(kFilterName), &CLSID_VirtualCameraFilter,
     &CreateFilterInstance, nullptr, nullptr},
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

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

extern "C" void vcam_dbg_trace(const char* text)
{
    vcamdriver::traceLine(text);
}
#endif

STDAPI DllRegisterServer()
{
    HRESULT hr = AMovieDllRegisterServer2(TRUE);
    if (FAILED(hr)) {
        return hr;
    }
    hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        return hr;
    }
    do {
        IFilterMapper2* mapper = nullptr;
        hr = CoCreateInstance(CLSID_FilterMapper2, nullptr,
                              CLSCTX_INPROC_SERVER, IID_IFilterMapper2,
                              reinterpret_cast<void**>(&mapper));
        if (FAILED(hr)) {
            break;
        }
        // Register as a camera (Video Input Device category). The instance
        // name makes the DevicePath readable ("Virtual Camera"); the
        // proper FilterData plus the CLSID value are written by Windows.
        mapper->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 0,
                                 CLSID_VirtualCameraFilter);
        hr = mapper->RegisterFilter(CLSID_VirtualCameraFilter, kFilterName, 0,
                                    &CLSID_VideoInputDeviceCategory,
                                    kFilterName, &kRegFilter2);
        mapper->Release();
    } while (false);
    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

STDAPI DllUnregisterServer()
{
    HRESULT hr = AMovieDllRegisterServer2(FALSE);
    if (FAILED(hr)) {
        return hr;
    }
    hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        return hr;
    }
    do {
        IFilterMapper2* mapper = nullptr;
        hr = CoCreateInstance(CLSID_FilterMapper2, nullptr,
                              CLSCTX_INPROC_SERVER, IID_IFilterMapper2,
                              reinterpret_cast<void**>(&mapper));
        if (FAILED(hr)) {
            break;
        }
        hr = mapper->UnregisterFilter(&CLSID_VideoInputDeviceCategory,
                                      kFilterName, CLSID_VirtualCameraFilter);
        mapper->Release();
    } while (false);
    CoFreeUnusedLibraries();
    CoUninitialize();
    return hr;
}

// The base classes' entry point (dllentry.cpp); DllMain forwards to it.
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
    return DllEntryPoint(reinterpret_cast<HINSTANCE>(hModule), dwReason,
                         lpReserved);
}

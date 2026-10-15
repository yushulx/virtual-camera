// dllmain.cpp - DLL entry point and the four exported COM functions.

#include "Common.h"

#include "ClassFactory.h"
#include "CameraFilter.h"

#include <cstdarg>
#include <cstdio>
#include <new>

namespace vcamdriver {

HMODULE g_module = nullptr;

#if defined(_DEBUG)
void trace(const wchar_t* fmt, ...)
{
    wchar_t buffer[512];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buffer, _TRUNCATE, fmt, args);
    va_end(args);
    OutputDebugStringW(buffer);
}
#endif

} // namespace vcamdriver

extern "C" BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        vcamdriver::g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid,
                                            void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (rclsid != CLSID_VirtualCameraFilter) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    auto* factory = new (std::nothrow) vcamdriver::ClassFactory();
    if (factory == nullptr) {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

extern "C" HRESULT WINAPI DllCanUnloadNow()
{
    return S_FALSE; // stay loaded; the DLL is tiny and used in-proc
}

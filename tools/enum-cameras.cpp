// enum-cameras.cpp - diagnostic tool: enumerates DirectShow video capture
// devices exactly like a browser does, and dumps the relevant registry keys.

#include <windows.h>
#include <objbase.h>
#include <dshow.h>
#include <stdio.h>

static void DumpHive(HKEY hive, const WCHAR* hiveName)
{
    const WCHAR* relative =
        L"CLSID\\{860BB310-5D01-11D0-BD3B-00A0C911CE86}\\Instance";
    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, relative, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        wprintf(L"  %s: (category instance key not present)\n", hiveName);
        return;
    }
    wprintf(L"  %s:\n", hiveName);
    DWORD index = 0;
    WCHAR name[256];
    DWORD nameLen = 256;
    while (RegEnumKeyExW(key, index++, name, &nameLen, nullptr, nullptr,
                         nullptr, nullptr) == ERROR_SUCCESS) {
        WCHAR subPath[512];
        swprintf_s(subPath,
                   L"CLSID\\{860BB310-5D01-11D0-BD3B-00A0C911CE86}\\Instance\\%s",
                   name);
        HKEY sub = nullptr;
        WCHAR friendly[256] = L"?";
        DWORD type = 0, size = sizeof(friendly);
        if (RegOpenKeyExW(hive, subPath, 0, KEY_READ, &sub) == ERROR_SUCCESS) {
            RegQueryValueExW(sub, L"FriendlyName", nullptr, &type,
                             (BYTE*)friendly, &size);
            DWORD filterDataSize = 0;
            RegQueryValueExW(sub, L"FilterData", nullptr, nullptr, nullptr,
                             &filterDataSize);
            wprintf(L"    %s -> \"%s\" (FilterData: %u bytes)\n", name,
                    friendly, filterDataSize);
            RegCloseKey(sub);
        }
        nameLen = 256;
    }
    RegCloseKey(key);
}

int main()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    printf("--- registry: capture category instance keys ---\n");
    DumpHive(HKEY_CLASSES_ROOT, L"HKCR (merged)");
    DumpHive(HKEY_LOCAL_MACHINE, L"HKLM");
    DumpHive(HKEY_CURRENT_USER, L"HKCU");

    printf("\n--- DirectShow enumeration (CLSID_VideoInputDeviceCategory) ---\n");

    ICreateDevEnum* devEnum = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
                                  (void**)&devEnum);
    if (FAILED(hr)) {
        printf("CoCreateInstance(SystemDeviceEnum) failed: 0x%08lX\n", hr);
        return 1;
    }
    IEnumMoniker* enumMoniker = nullptr;
    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                        &enumMoniker, 0);
    devEnum->Release();
    if (hr == S_FALSE) {
        printf("NO capture devices found (S_FALSE).\n");
        return 2;
    }
    if (FAILED(hr)) {
        printf("CreateClassEnumerator failed: 0x%08lX\n", hr);
        return 1;
    }

    IMoniker* moniker = nullptr;
    int count = 0;
    while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
        IPropertyBag* bag = nullptr;
        hr = moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                    (void**)&bag);
        if (SUCCEEDED(hr)) {
            VARIANT var;
            VariantInit(&var);
            if (SUCCEEDED(bag->Read(L"FriendlyName", &var, nullptr))) {
                wprintf(L"  [%d] FriendlyName: %s\n", count, var.bstrVal);
                VariantClear(&var);
            }
            if (SUCCEEDED(bag->Read(L"DevicePath", &var, nullptr))) {
                wprintf(L"       DevicePath:   %s\n", var.bstrVal);
                VariantClear(&var);
            }
            bag->Release();
        }
        LPOLESTR displayName = nullptr;
        if (SUCCEEDED(moniker->GetDisplayName(nullptr, nullptr, &displayName))) {
            wprintf(L"       DisplayName:  %s\n", displayName);
            CoTaskMemFree(displayName);
        }
        // Bind the filter to prove it can be instantiated (like a browser).
        IBaseFilter* filter = nullptr;
        hr = moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter,
                                   (void**)&filter);
        if (SUCCEEDED(hr)) {
            FILTER_INFO info{};
            if (SUCCEEDED(filter->QueryFilterInfo(&info))) {
                wprintf(L"       BindToObject: OK -> \"%s\"\n", info.achName);
                if (info.pGraph) info.pGraph->Release();
            }
            filter->Release();
        } else {
            wprintf(L"       BindToObject: FAILED 0x%08lX\n", hr);
        }
        moniker->Release();
        ++count;
    }
    enumMoniker->Release();
    printf("%d device(s) found.\n", count);

    // --- direct activation by CLSID (no devenum) --------------------------
    printf("\n--- direct activation by CLSID (no devenum) ---\n");
    static const CLSID kVirtualCameraFilterClsid = {
        0xd4a1f5b2, 0x6e38, 0x4c7a,
        {0x9b, 0x2e, 0x1f, 0x8d, 0x3a, 0x5c, 0x7e, 0x64}};
    IBaseFilter* directFilter = nullptr;
    hr = CoCreateInstance(kVirtualCameraFilterClsid, nullptr,
                          CLSCTX_INPROC_SERVER, IID_IBaseFilter,
                          (void**)&directFilter);
    printf("CoCreateInstance: 0x%08lX %s\n", hr,
           SUCCEEDED(hr) ? "(OK)" : "(FAILED)");
    if (SUCCEEDED(hr)) {
        directFilter->Release();
    }
    return 0;
}

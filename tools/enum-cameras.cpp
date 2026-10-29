// enum-cameras.cpp - diagnostic tool: enumerates DirectShow video capture
// devices exactly like a browser does, and dumps the relevant registry keys.
//
// The extra per-device detail (CLSID, server DLL, architecture, 32-bit view)
// is what separates the usual "the camera is not listed" causes:
//   * the filter is not registered at all,
//   * it is registered for the other bitness than the host application,
//   * the host application enumerated devices before the filter existed.

#include <windows.h>
#include <objbase.h>
#include <dshow.h>
#include <stdio.h>

static const wchar_t kCategoryKey[] =
    L"CLSID\\{860BB310-5D01-11D0-BD3B-00A0C911CE86}\\Instance";

static const wchar_t kOurClsid[] = L"{D4A1F5B2-6E38-4C7A-9B2E-1F8D3A5C7E64}";
static const wchar_t kObsClsid[] = L"{A3FCE0F5-3493-419F-958A-ABA1250EC20B}";

struct PeInfo {
    const char* arch = "unknown";
    bool ok = false;
};

static PeInfo ReadPeArchitecture(const wchar_t* path)
{
    PeInfo info;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return info;
    }
    IMAGE_DOS_HEADER dos{};
    DWORD read = 0;
    if (ReadFile(file, &dos, sizeof(dos), &read, nullptr) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE &&
        SetFilePointer(file, dos.e_lfanew, nullptr, FILE_BEGIN) !=
            INVALID_SET_FILE_POINTER) {
        DWORD signature = 0;
        if (ReadFile(file, &signature, sizeof(signature), &read, nullptr) &&
            signature == IMAGE_NT_SIGNATURE) {
            IMAGE_FILE_HEADER header{};
            if (ReadFile(file, &header, sizeof(header), &read, nullptr)) {
                info.ok = true;
                switch (header.Machine) {
                case IMAGE_FILE_MACHINE_AMD64: info.arch = "x64"; break;
                case IMAGE_FILE_MACHINE_I386:  info.arch = "x86"; break;
                case IMAGE_FILE_MACHINE_ARM64: info.arch = "arm64"; break;
                default: info.arch = "other"; break;
                }
            }
        }
    }
    CloseHandle(file);
    return info;
}

static bool ReadServerPath(const wchar_t* clsid, wchar_t* out, DWORD outChars)
{
    wchar_t keyPath[512];
    swprintf_s(keyPath, L"CLSID\\%s\\InprocServer32", clsid);
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    DWORD size = outChars * sizeof(wchar_t);
    const LONG rc =
        RegQueryValueExW(key, nullptr, nullptr, &type, (BYTE*)out, &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

static bool HasWow6432Registration(const wchar_t* clsid)
{
    wchar_t keyPath[512];
    swprintf_s(keyPath,
               L"SOFTWARE\\WOW6432Node\\Classes\\CLSID\\%s\\InprocServer32",
               clsid);
    HKEY key = nullptr;
    const bool exists = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ,
                                      &key) == ERROR_SUCCESS;
    if (exists) {
        RegCloseKey(key);
    }
    return exists;
}

static void DumpHive(HKEY hive, const WCHAR* hiveName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, kCategoryKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
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
        swprintf_s(subPath, L"%s\\%s", kCategoryKey, name);
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

static void PrintDeviceDetail(IPropertyBag* bag)
{
    VARIANT var;
    VariantInit(&var);
    if (FAILED(bag->Read(L"CLSID", &var, nullptr)) || var.bstrVal == nullptr) {
        VariantClear(&var);
        return;
    }
    const wchar_t* clsid = var.bstrVal;
    wchar_t server[MAX_PATH] = L"";
    if (ReadServerPath(clsid, server, MAX_PATH)) {
        const PeInfo pe = ReadPeArchitecture(server);
        wprintf(L"       CLSID:       %s\n", clsid);
        wprintf(L"       Server:      %s (%hs)\n", server,
                pe.ok ? pe.arch : "missing");
    } else {
        wprintf(L"       CLSID:       %s (no InprocServer32!)\n", clsid);
    }
    wprintf(L"       32-bit reg:  %s\n",
            HasWow6432Registration(clsid) ? L"yes" : L"no");
    VariantClear(&var);
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
        enumMoniker = nullptr;
    }
    if (FAILED(hr) && hr != S_FALSE) {
        printf("CreateClassEnumerator failed: 0x%08lX\n", hr);
        return 1;
    }

    IMoniker* moniker = nullptr;
    int count = 0;
    while (enumMoniker != nullptr &&
           enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
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
            PrintDeviceDetail(bag);
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
    if (enumMoniker != nullptr) {
        enumMoniker->Release();
    }
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

    // --- verdict ---------------------------------------------------------
    printf("\n--- summary ---\n");
    wchar_t server[MAX_PATH] = L"";
    if (ReadServerPath(kOurClsid, server, MAX_PATH)) {
        const PeInfo pe = ReadPeArchitecture(server);
        wprintf(L"  Virtual Camera (this project): registered, %hs\n",
                pe.ok ? pe.arch : "?");
        wprintf(L"    %s\n", server);
    } else {
        printf("  Virtual Camera (this project): NOT registered\n");
    }
    if (ReadServerPath(kObsClsid, server, MAX_PATH)) {
        wprintf(L"  OBS Virtual Camera:            registered\n    %s\n", server);
    } else {
        printf("  OBS Virtual Camera:            not installed\n");
    }
    printf("  Enumerated by this %s process: %d device(s)\n",
           sizeof(void*) == 8 ? "64-bit" : "32-bit", count);
    printf("  A 32-bit application sees its own list; this filter is\n"
           "  registered for 64-bit consumers only.\n");
    return 0;
}

// Registry.cpp - self registration for regsvr32 / the Inno Setup installer.
//
// Registers the DirectShow capture source filter under the
// "Video Input Devices" category so it shows up as a camera in every
// DirectShow application (Chrome, Edge, Firefox, Zoom, OBS, ...).

#include "Common.h"

#include <string>
#include <vector>

namespace vcamdriver {
namespace {

// REG_PINFLAG_B_OUTPUT: the single pin is an output pin.
constexpr DWORD kPinFlagOutput = 0x2;
// MERIT_DO_NOT_USE: devices are enumerated explicitly by category, not by
// merit-based graph building.
constexpr DWORD kMeritDoNotUse = 0x200000;

void appendDword(std::vector<BYTE>& blob, DWORD value)
{
    for (int i = 0; i < 4; ++i) {
        blob.push_back(static_cast<BYTE>((value >> (i * 8)) & 0xFF));
    }
}

void appendGuid(std::vector<BYTE>& blob, const GUID& guid)
{
    appendDword(blob, guid.Data1);
    appendDword(blob, (static_cast<DWORD>(guid.Data2) << 16) | guid.Data3);
    for (int i = 0; i < 8; ++i) {
        blob.push_back(guid.Data4[i]);
    }
}

// FilterData is the serialized REGFILTER2 structure (see "Filter Data" in
// the DirectShow documentation): version, merit, pin count, then the pins
// with their media types as inline GUIDs.
std::vector<BYTE> buildFilterData()
{
    std::vector<BYTE> blob;
    blob.reserve(128);
    appendDword(blob, 2);            // dwVersion: REGFILTER2 with REGFILTERPINS2
    appendDword(blob, kMeritDoNotUse); // dwMerit
    appendDword(blob, 1);            // cPins
    // REGFILTERPINS2[0]:
    appendDword(blob, kPinFlagOutput); // dwFlags: output
    appendDword(blob, 1);              // cInstances
    appendDword(blob, 1);              // nMediaTypes
    appendGuid(blob, MEDIATYPE_Video);
    appendGuid(blob, MEDIASUBTYPE_YUY2);
    appendDword(blob, 0);              // nMediums
    appendGuid(blob, VCAM_PIN_CATEGORY_CAPTURE);
    return blob;
}

LSTATUS setValue(HKEY root, const wchar_t* subkey, const wchar_t* valueName,
                DWORD type, const void* data, DWORD size)
{
    HKEY key = nullptr;
    LSTATUS status =
        RegCreateKeyExW(root, subkey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        return status;
    }
    status = RegSetValueExW(key, valueName, 0, type,
                            static_cast<const BYTE*>(data), size);
    RegCloseKey(key);
    return status;
}

std::wstring guidToString(REFGUID guid)
{
    wchar_t buffer[64] = {};
    StringFromGUID2(guid, buffer, 64);
    return std::wstring(buffer);
}

bool registerServer()
{
    wchar_t dllPath[MAX_PATH] = {};
    if (GetModuleFileNameW(g_module, dllPath, MAX_PATH) == 0) {
        return false;
    }
    const std::wstring clsidKey =
        L"CLSID\\" + guidToString(CLSID_VirtualCameraFilter);
    const std::wstring instanceKey =
        L"CLSID\\" + guidToString(VCAM_CLSID_VideoInputDeviceCategory) +
        L"\\Instance\\" + guidToString(CLSID_VirtualCameraFilter);
    const wchar_t* friendlyName = kCameraName;
    const std::vector<BYTE> filterData = buildFilterData();
    const wchar_t* devicePath = L"virtualcamera:";

    if (setValue(HKEY_CLASSES_ROOT, clsidKey.c_str(), nullptr, REG_SZ,
                 friendlyName,
                 static_cast<DWORD>((wcslen(friendlyName) + 1) *
                                    sizeof(wchar_t))) != ERROR_SUCCESS) {
        return false;
    }
    if (setValue(HKEY_CLASSES_ROOT, (clsidKey + L"\\InprocServer32").c_str(),
                 nullptr, REG_SZ, dllPath,
                 static_cast<DWORD>((wcslen(dllPath) + 1) *
                                    sizeof(wchar_t))) != ERROR_SUCCESS) {
        return false;
    }
    const wchar_t* threadingModel = L"Both";
    if (setValue(HKEY_CLASSES_ROOT, (clsidKey + L"\\InprocServer32").c_str(),
                 L"ThreadingModel", REG_SZ, threadingModel,
                 static_cast<DWORD>((wcslen(threadingModel) + 1) *
                                    sizeof(wchar_t))) != ERROR_SUCCESS) {
        return false;
    }
    if (setValue(HKEY_CLASSES_ROOT, instanceKey.c_str(), L"FriendlyName",
                 REG_SZ, friendlyName,
                 static_cast<DWORD>((wcslen(friendlyName) + 1) *
                                    sizeof(wchar_t))) != ERROR_SUCCESS) {
        return false;
    }
    if (setValue(HKEY_CLASSES_ROOT, instanceKey.c_str(), L"FilterData",
                 REG_BINARY, filterData.data(),
                 static_cast<DWORD>(filterData.size())) != ERROR_SUCCESS) {
        return false;
    }
    if (setValue(HKEY_CLASSES_ROOT, instanceKey.c_str(), L"DevicePath", REG_SZ,
                 devicePath,
                 static_cast<DWORD>((wcslen(devicePath) + 1) *
                                    sizeof(wchar_t))) != ERROR_SUCCESS) {
        return false;
    }
    return true;
}

void unregisterServer()
{
    const std::wstring instanceKey =
        L"CLSID\\" + guidToString(VCAM_CLSID_VideoInputDeviceCategory) +
        L"\\Instance\\" + guidToString(CLSID_VirtualCameraFilter);
    const std::wstring clsidKey =
        L"CLSID\\" + guidToString(CLSID_VirtualCameraFilter);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, instanceKey.c_str());
    RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidKey.c_str());
}

} // namespace

} // namespace vcamdriver

using namespace vcamdriver;

extern "C" HRESULT WINAPI DllRegisterServer()
{
    return registerServer() ? S_OK : SELFREG_E_CLASS;
}

extern "C" HRESULT WINAPI DllUnregisterServer()
{
    unregisterServer();
    return S_OK;
}

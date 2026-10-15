// graph-test.cpp - builds the same capture graph a browser would build
// (RenderStream on PIN_CATEGORY_CAPTURE into a sample grabber + null
// renderer), runs it and reports the delivered frames. Verifies the filter
// end-to-end without a browser.
//
// qedit.h (SampleGrabber) was removed from the Windows SDK, so the small
// interface bits are declared here.

#include <windows.h>
#include <objbase.h>
#include <dshow.h>
#include <stdio.h>

struct __declspec(uuid("6B652FFF-11FE-4FCE-92AD-0266B5D7C78F"))
    ISampleGrabber : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL OneShot) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE* pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE* pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL BufferThem) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long* pBufferSize,
                                                       long* pBuffer) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(long** ppSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(long* CallBackMethod,
                                                  long WhichMethodToCallback) = 0;
};

// {C1F400A0-3F08-11D3-9F0B-006008039E37}
static const CLSID kCLSID_NullRenderer = {
    0xc1f400a0, 0x3f08, 0x11d3, {0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37}};
// {C1F400A4-3F08-11D3-9F0B-006008039E37}
static const CLSID kCLSID_SampleGrabber = {
    0xc1f400a4, 0x3f08, 0x11d3, {0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37}};

static const CLSID kVirtualCameraFilterClsid = {
    0xd4a1f5b2, 0x6e38, 0x4c7a, {0x9b, 0x2e, 0x1f, 0x8d, 0x3a, 0x5c, 0x7e, 0x64}};

int wmain(int argc, WCHAR** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    // Watchdog: convert hangs into a visible report.
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        for (int i = 1; i <= 15; ++i) {
            Sleep(1000);
            printf("[watchdog %ds alive]\n", i);
        }
        printf("WATCHDOG TIMEOUT - the last step hung\n");
        ExitProcess(43);
        return 0;
    }, nullptr, 0, nullptr);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IBaseFilter* camera = nullptr;
    HRESULT hr = E_FAIL;
    if (argc > 2 && wcscmp(argv[1], L"--dll") == 0) {
        // Instantiate from a specific DLL via its DllGetClassObject export
        // (bypasses the registry, so an unregistered build can be tested).
        HMODULE module = LoadLibraryW(argv[2]);
        if (!module) {
            printf("LoadLibrary failed: %lu\n", GetLastError());
            return 1;
        }
        using DllGetClassObjectFn = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
        auto get = reinterpret_cast<DllGetClassObjectFn>(
            GetProcAddress(module, "DllGetClassObject"));
        if (!get) {
            printf("DllGetClassObject export not found\n");
            return 1;
        }
        IClassFactory* factory = nullptr;
        hr = get(kVirtualCameraFilterClsid, IID_IClassFactory,
                 reinterpret_cast<void**>(&factory));
        if (FAILED(hr)) {
            printf("DllGetClassObject: 0x%08lX\n", hr);
            return 1;
        }
        hr = factory->CreateInstance(nullptr, IID_IBaseFilter,
                                     reinterpret_cast<void**>(&camera));
        factory->Release();
        printf("CreateInstance from provided DLL: 0x%08lX\n", hr);
    } else {
        hr = CoCreateInstance(kVirtualCameraFilterClsid, nullptr,
                              CLSCTX_INPROC_SERVER, IID_IBaseFilter,
                              (void**)&camera);
    }
    printf("filter created: 0x%08lX\n", hr);
    if (FAILED(hr)) {
        return 1;
    }

    // Find the capture pin like WebRTC does: IKsPropertySet pin category.
    IEnumPins* enumPins = nullptr;
    camera->EnumPins(&enumPins);
    IPin* capturePin = nullptr;
    IPin* pin = nullptr;
    while (enumPins && enumPins->Next(1, &pin, nullptr) == S_OK) {
        PIN_DIRECTION dir = PINDIR_INPUT;
        pin->QueryDirection(&dir);
        if (dir != PINDIR_OUTPUT) {
            pin->Release();
            continue;
        }
        IKsPropertySet* ks = nullptr;
        if (SUCCEEDED(pin->QueryInterface(IID_IKsPropertySet, (void**)&ks))) {
            GUID category{};
            DWORD returned = 0;
            hr = ks->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, nullptr, 0,
                         &category, sizeof(category), &returned);
            ks->Release();
            static const GUID kPinCategoryCapture = {
                0xfb6c4281, 0x353, 0x11d1, {0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba}};
            if (SUCCEEDED(hr) && IsEqualGUID(category, kPinCategoryCapture)) {
                capturePin = pin;
                printf("capture pin found via IKsPropertySet (PIN_CATEGORY_CAPTURE)\n");
                break;
            }
        }
        pin->Release();
    }
    if (enumPins) enumPins->Release();
    if (!capturePin) {
        printf("FAILED: no capture pin discoverable via IKsPropertySet - "
               "browsers cannot start this camera (NotReadableError)\n");
        camera->Release();
        return 2;
    }
    capturePin->Release();

    printf("step: creating filter graph\n");
    IGraphBuilder* graph = nullptr;
    ICaptureGraphBuilder2* builder = nullptr;
    IBaseFilter* grabber = nullptr;
    IBaseFilter* renderer = nullptr;
    ISampleGrabber* sampleGrabber = nullptr;
    IMediaControl* control = nullptr;
    hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IGraphBuilder, (void**)&graph);
    if (SUCCEEDED(hr))
        hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr,
                              CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
                              (void**)&builder);
    if (SUCCEEDED(hr))
        hr = CoCreateInstance(kCLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IBaseFilter, (void**)&grabber);
    printf("step: sample grabber created\n");
    if (SUCCEEDED(hr))
        hr = grabber->QueryInterface(__uuidof(ISampleGrabber), (void**)&sampleGrabber);
    if (SUCCEEDED(hr) && sampleGrabber != nullptr) {
        // No SetMediaType: let the grabber accept any video type (setting a
        // template with an empty formattype makes CheckMediaType reject our
        // FORMAT_VideoInfo type).
        sampleGrabber->SetBufferSamples(TRUE);
    }
    if (SUCCEEDED(hr))
        hr = CoCreateInstance(kCLSID_NullRenderer, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IBaseFilter, (void**)&renderer);
    if (SUCCEEDED(hr))
        hr = graph->AddFilter(camera, L"Virtual Camera");
    printf("step: AddFilter(camera) done\n");
    if (SUCCEEDED(hr))
        hr = graph->AddFilter(grabber, L"Grabber");
    if (SUCCEEDED(hr))
        hr = graph->AddFilter(renderer, L"Null Renderer");
    if (SUCCEEDED(hr))
        hr = builder->SetFiltergraph(graph);
    printf("step: SetFiltergraph done\n");

    // Diagnose the pin lookup RenderStream will perform.
    IPin* foundPin = nullptr;
    hr = builder->FindPin(camera, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE,
                          &MEDIATYPE_Video, FALSE, 0, &foundPin);
    printf("FindPin(PIN_CATEGORY_CAPTURE): 0x%08lX %s\n", hr,
           SUCCEEDED(hr) ? "(OK)" : "(FAILED)");
    if (foundPin) {
        foundPin->Release();
        foundPin = nullptr;
    }

    // Explicit media type: RGB24 1280x720 (bypasses negotiation; a browser
    // ultimately does the same via IAMStreamConfig SetFormat).
    printf("step: ConnectDirect with explicit RGB24 media type\n");
    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)CoTaskMemAlloc(sizeof(VIDEOINFOHEADER));
    ZeroMemory(vih, sizeof(*vih));
    const int w = 1280, h = 720;
    const int stride = (w * 3 + 3) & ~3;
    vih->dwBitRate = (DWORD)((INT64)w * h * 24 * 30);
    vih->AvgTimePerFrame = 333333;
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth = w;
    vih->bmiHeader.biHeight = h;
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 24;
    vih->bmiHeader.biCompression = BI_RGB;
    vih->bmiHeader.biSizeImage = stride * h;
    AM_MEDIA_TYPE mt{};
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    mt.bFixedSizeSamples = TRUE;
    mt.lSampleSize = stride * h;
    mt.formattype = FORMAT_VideoInfo;
    mt.cbFormat = sizeof(VIDEOINFOHEADER);
    mt.pbFormat = (BYTE*)vih;

    {
        IEnumPins* rp = nullptr;
        grabber->EnumPins(&rp);
        IPin* sinkPin = nullptr;
        if (rp) rp->Next(1, &sinkPin, nullptr);
        if (rp) rp->Release();
        if (!sinkPin) { printf("no grabber pin\n"); return 3; }
        hr = graph->ConnectDirect(capturePin, sinkPin, &mt);
        printf("ConnectDirect(explicit RGB24): 0x%08lX\n", hr);
        sinkPin->Release();
        if (FAILED(hr)) return 3;
    }
    if (SUCCEEDED(hr))
        hr = graph->QueryInterface(IID_IMediaControl, (void**)&control);
    if (SUCCEEDED(hr))
        hr = control->Run();
    if (SUCCEEDED(hr)) {
        OAFilterState state = State_Stopped;
        control->GetState(3000, &state);
        printf("graph state after Run: %d (4 = running)\n", state);
    } else if (control) {
        printf("Run: 0x%08lX\n", hr);
    }

    Sleep(1000);
    long size = 0;
    hr = sampleGrabber ? sampleGrabber->GetCurrentBuffer(&size, nullptr) : E_FAIL;
    if (SUCCEEDED(hr) && size > 0) {
        BYTE* buffer = static_cast<BYTE*>(CoTaskMemAlloc(size));
        sampleGrabber->GetCurrentBuffer(&size, (long*)buffer);
        printf("delivered frame: %ld bytes (expected %d for 1280x720 YUY2)\n",
               size, 1280 * 720 * 2);
        CoTaskMemFree(buffer);
    } else {
        printf("GetCurrentBuffer: 0x%08lX (no frame delivered)\n", hr);
    }
    if (control) control->Stop();

    if (control) control->Release();
    if (sampleGrabber) sampleGrabber->Release();
    if (renderer) renderer->Release();
    if (grabber) grabber->Release();
    if (builder) builder->Release();
    if (graph) graph->Release();
    camera->Release();
    CoUninitialize();
    printf("done\n");
    return 0;
}

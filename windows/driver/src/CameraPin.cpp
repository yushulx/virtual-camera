#include "CameraPin.h"

#include "CameraFilter.h"
#include "EnumMediaTypes.h"
#include "MemAllocator.h"

#include <new>

namespace vcamdriver {

namespace {

// --- built-in test pattern (served when the controller is not running) ----
// 8 color bars in YUYV plus a moving bright marker proving liveness.

struct YuvColor
{
    uint8_t y, u, v;
};

const YuvColor kBars[8] = {
    {180, 128, 128}, // light gray
    {165, 44, 172},  // yellow
    {135, 84, 255},  // cyan
    {112, 72, 58},   // green
    {84, 184, 198},  // magenta
    {65, 100, 212},  // red
    {35, 212, 114},  // blue
    {16, 128, 128},  // black
};

void drawTestPattern(uint8_t* yuyv, uint32_t width, uint32_t height,
                     uint64_t frameIndex)
{
    const uint32_t markerX = static_cast<uint32_t>(frameIndex % width);
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t* row = yuyv + static_cast<size_t>(y) * width * 2;
        for (uint32_t x = 0; x < width; x += 2) {
            const uint32_t bar = x * 8 / width;
            const YuvColor& color = kBars[bar % 8];
            const YuvColor& next = kBars[(x + 1) * 8 / width % 8];
            row[0] = color.y;
            row[1] = color.u;
            row[2] = next.y;
            row[3] = color.v;
            row += 4;
        }
    }
    // Moving marker: a white column sweeping across the bars.
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t* row = yuyv + static_cast<size_t>(y) * width * 2;
        for (uint32_t dx = 0; dx < 8 && markerX + dx < width; ++dx) {
            uint8_t* pixel = row + static_cast<size_t>(markerX + dx) * 2;
            pixel[0] = 235;
            pixel[1] = 128;
            if (markerX + dx + 1 < width) {
                pixel[2] = 235;
                pixel[3] = 128;
            }
        }
    }
}

int64_t qpcNow()
{
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

int64_t qpcFrequency()
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

} // namespace

CameraPin::CameraPin(CameraFilter* filter)
    : m_filter(filter)
{
    m_filter->AddRef();
    m_format = currentDesiredFormat();
}

CameraPin::~CameraPin()
{
    // Nothing left to do after shutdown(); release the filter reference.
    m_filter->Release();
}

// --- IUnknown ----------------------------------------------------------------

STDMETHODIMP CameraPin::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown) {
        *ppv = static_cast<IPin*>(this);
    } else if (riid == IID_IPin) {
        *ppv = static_cast<IPin*>(this);
    } else if (riid == IID_IAMStreamConfig) {
        *ppv = static_cast<IAMStreamConfig*>(this);
    } else {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CameraPin::AddRef()
{
    return InterlockedIncrement(&m_refs);
}

STDMETHODIMP_(ULONG) CameraPin::Release()
{
    const ULONG refs = InterlockedDecrement(&m_refs);
    if (refs == 0) {
        shutdown();
        delete this;
    }
    return refs;
}

// --- IPin --------------------------------------------------------------------

CameraFormat CameraPin::currentDesiredFormat() const
{
    uint32_t width = 0, height = 0, fps = 0;
    // Note: readHeader opens/validates the mapping lazily; const in spirit.
    if (const_cast<SharedFrameReader&>(m_reader).readHeader(&width, &height,
                                                             &fps)) {
        CameraFormat format;
        format.width = width;
        format.height = height;
        format.fps = fps;
        return format;
    }
    CameraFormat format;
    format.width = vcam::kDefaultWidth;
    format.height = vcam::kDefaultHeight;
    format.fps = vcam::kDefaultFps;
    return format;
}

bool CameraPin::currentMediaType(AM_MEDIA_TYPE* mt)
{
    return makeMediaType(currentDesiredFormat(), mt);
}

STDMETHODIMP CameraPin::Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt)
{
    if (pReceivePin == nullptr) {
        return E_POINTER;
    }
    if (m_connected != nullptr) {
        return VFW_E_ALREADY_CONNECTED;
    }
    PIN_DIRECTION direction = PINDIR_OUTPUT;
    if (FAILED(pReceivePin->QueryDirection(&direction)) ||
        direction != PINDIR_INPUT) {
        return E_UNEXPECTED;
    }

    // Pick up the format the controller advertises right now (the one the
    // consumer saw during enumeration), then negotiate.
    m_format = currentDesiredFormat();
    AM_MEDIA_TYPE mt{};
    if (!makeMediaType(m_format, &mt)) {
        return E_OUTOFMEMORY;
    }

    if (pmt != nullptr) {
        CameraFormat requested{};
        if (!formatFromMediaType(pmt, &requested) ||
            requested.width != m_format.width ||
            requested.height != m_format.height) {
            freeMediaType(&mt);
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
        m_format.fps = requested.fps ? requested.fps : m_format.fps;
        freeMediaType(&mt);
        if (!makeMediaType(m_format, &mt)) {
            return E_OUTOFMEMORY;
        }
    }

    // Ask the downstream input pin to accept the connection.
    HRESULT hr = pReceivePin->ReceiveConnection(this, &mt);
    if (FAILED(hr)) {
        freeMediaType(&mt);
        return hr;
    }

    // Our own allocator: 3 buffers of exactly one frame each.
    auto* allocator = new (std::nothrow) MemAllocator();
    if (allocator == nullptr) {
        freeMediaType(&mt);
        return E_OUTOFMEMORY;
    }
    ALLOCATOR_PROPERTIES request{};
    request.cBuffers = 3;
    request.cbBuffer = static_cast<LONG>(m_format.width * m_format.height * 2);
    request.cbAlign = 1;
    ALLOCATOR_PROPERTIES actual{};
    if (FAILED(allocator->SetProperties(&request, &actual)) ||
        FAILED(allocator->Commit()) ||
        FAILED(pReceivePin->NotifyAllocator(allocator, FALSE))) {
        allocator->Decommit();
        allocator->Release();
        freeMediaType(&mt);
        return VFW_E_NO_ALLOCATOR;
    }

    m_connected = pReceivePin;
    m_connected->AddRef();
    m_allocator = allocator; // we keep our reference
    m_mt = mt;               // take ownership
    m_frameIndex = 0;
    VCAM_TRACE(L"CameraPin::Connect %ux%u @ %u fps OK\n", m_format.width,
               m_format.height, m_format.fps);
    return S_OK;
}

STDMETHODIMP CameraPin::ReceiveConnection(IPin*, const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED; // we are an output pin
}

STDMETHODIMP CameraPin::Disconnect()
{
    if (m_connected == nullptr) {
        return S_FALSE;
    }
    Inactive();
    m_connected->Release();
    m_connected = nullptr;
    if (m_allocator) {
        m_allocator->Decommit();
        m_allocator->Release();
        m_allocator = nullptr;
    }
    freeMediaType(&m_mt);
    ZeroMemory(&m_mt, sizeof(AM_MEDIA_TYPE));
    return S_OK;
}

STDMETHODIMP CameraPin::ConnectedTo(IPin** pPin)
{
    if (pPin == nullptr) {
        return E_POINTER;
    }
    if (m_connected == nullptr) {
        *pPin = nullptr;
        return VFW_E_NOT_CONNECTED;
    }
    *pPin = m_connected;
    (*pPin)->AddRef();
    return S_OK;
}

STDMETHODIMP CameraPin::ConnectionMediaType(AM_MEDIA_TYPE* pmt)
{
    if (pmt == nullptr) {
        return E_POINTER;
    }
    ZeroMemory(pmt, sizeof(AM_MEDIA_TYPE));
    if (m_connected == nullptr) {
        return VFW_E_NOT_CONNECTED;
    }
    return copyMediaType(pmt, &m_mt) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP CameraPin::QueryPinInfo(PIN_INFO* pInfo)
{
    if (pInfo == nullptr) {
        return E_POINTER;
    }
    pInfo->pFilter = static_cast<IBaseFilter*>(m_filter);
    pInfo->pFilter->AddRef();
    pInfo->dir = PINDIR_OUTPUT;
    wcscpy_s(pInfo->achName, vcamdriver::kCameraName);
    return S_OK;
}

STDMETHODIMP CameraPin::QueryDirection(PIN_DIRECTION* pPinDir)
{
    if (pPinDir == nullptr) {
        return E_POINTER;
    }
    *pPinDir = PINDIR_OUTPUT;
    return S_OK;
}

STDMETHODIMP CameraPin::QueryId(LPWSTR* Id)
{
    if (Id == nullptr) {
        return E_POINTER;
    }
    *Id = static_cast<LPWSTR>(CoTaskMemAlloc(sizeof(wchar_t) * 2));
    if (*Id == nullptr) {
        return E_OUTOFMEMORY;
    }
    wcscpy_s(*Id, 2, L"1");
    return S_OK;
}

STDMETHODIMP CameraPin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    if (pmt == nullptr) {
        return E_POINTER;
    }
    CameraFormat format{};
    if (!formatFromMediaType(pmt, &format)) {
        return S_FALSE;
    }
    const CameraFormat desired = currentDesiredFormat();
    return format.width == desired.width && format.height == desired.height
               ? S_OK
               : S_FALSE;
}

STDMETHODIMP CameraPin::EnumMediaTypes(IEnumMediaTypes** ppEnum)
{
    if (ppEnum == nullptr) {
        return E_POINTER;
    }
    auto* enumerator = new (std::nothrow) EnumMediaTypes(this);
    if (enumerator == nullptr) {
        return E_OUTOFMEMORY;
    }
    *ppEnum = enumerator;
    return S_OK;
}

STDMETHODIMP CameraPin::QueryInternalConnections(IPin*, ULONG*)
{
    return E_NOTIMPL; // no internal connections
}

STDMETHODIMP CameraPin::BeginFlush()
{
    return S_OK; // sources have nothing to flush downstream
}

STDMETHODIMP CameraPin::EndFlush()
{
    return S_OK;
}

STDMETHODIMP CameraPin::EndOfStream()
{
    return E_UNEXPECTED; // not called on output pins
}

// --- IAMStreamConfig ----------------------------------------------------------

STDMETHODIMP CameraPin::SetFormat(AM_MEDIA_TYPE* pmt)
{
    if (pmt == nullptr) {
        return E_POINTER;
    }
    if (m_connected != nullptr) {
        return VFW_E_WRONG_STATE;
    }
    CameraFormat format{};
    if (!formatFromMediaType(pmt, &format)) {
        return VFW_E_INVALIDMEDIATYPE;
    }
    const CameraFormat desired = currentDesiredFormat();
    if (format.width != desired.width || format.height != desired.height) {
        // The controller produces exactly one resolution; the user changes
        // it in the Virtual Camera application.
        return VFW_E_INVALIDMEDIATYPE;
    }
    m_format = desired;
    return S_OK;
}

STDMETHODIMP CameraPin::GetFormat(AM_MEDIA_TYPE** ppmt)
{
    if (ppmt == nullptr) {
        return E_POINTER;
    }
    AM_MEDIA_TYPE mt{};
    if (!makeMediaType(currentDesiredFormat(), &mt)) {
        return E_OUTOFMEMORY;
    }
    *ppmt = static_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    if (*ppmt == nullptr) {
        freeMediaType(&mt);
        return E_OUTOFMEMORY;
    }
    **ppmt = mt;
    return S_OK;
}

STDMETHODIMP CameraPin::GetNumberOfCapabilities(int* piCount, int* piSize)
{
    if (piCount == nullptr || piSize == nullptr) {
        return E_POINTER;
    }
    *piCount = 1;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

STDMETHODIMP CameraPin::GetStreamCaps(int iIndex, BYTE* pSCC,
                                      AM_MEDIA_TYPE** ppmt)
{
    if (iIndex != 0) {
        return E_INVALIDARG;
    }
    if (pSCC == nullptr) {
        return E_POINTER;
    }
    const CameraFormat format = currentDesiredFormat();

    if (ppmt != nullptr && *ppmt == nullptr) {
        AM_MEDIA_TYPE mt{};
        if (!makeMediaType(format, &mt)) {
            return E_OUTOFMEMORY;
        }
        *ppmt = static_cast<AM_MEDIA_TYPE*>(
            CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
        if (*ppmt == nullptr) {
            freeMediaType(&mt);
            return E_OUTOFMEMORY;
        }
        **ppmt = mt;
    }

    auto* caps = reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(pSCC);
    ZeroMemory(caps, sizeof(VIDEO_STREAM_CONFIG_CAPS));
    caps->guid = MEDIASUBTYPE_YUY2;
    caps->VideoStandard = 0;
    caps->InputSize.cx = static_cast<LONG>(format.width);
    caps->InputSize.cy = static_cast<LONG>(format.height);
    caps->MinCroppingSize = caps->InputSize;
    caps->MaxCroppingSize = caps->InputSize;
    caps->MinOutputSize = caps->InputSize;
    caps->MaxOutputSize = caps->InputSize;
    caps->MinFrameInterval =
        static_cast<REFERENCE_TIME>(10000000i64 / format.fps);
    caps->MaxFrameInterval = caps->MinFrameInterval;
    caps->MinBitsPerSecond = format.width * format.height * 2 * 8 * format.fps;
    caps->MaxBitsPerSecond = caps->MinBitsPerSecond;
    return S_OK;
}

// --- delivery thread ----------------------------------------------------------

HRESULT CameraPin::Active()
{
    if (m_connected == nullptr || m_allocator == nullptr) {
        return S_FALSE; // nothing to deliver into yet
    }
    if (m_allocator != nullptr) {
        m_allocator->Commit();
    }
    InterlockedExchange(&m_runFlag, 1);
    if (m_thread == nullptr) {
        m_thread = CreateThread(nullptr, 0, &CameraPin::threadEntry, this, 0,
                                nullptr);
        if (m_thread == nullptr) {
            InterlockedExchange(&m_runFlag, 0);
            return E_FAIL;
        }
    }
    return S_OK;
}

HRESULT CameraPin::Inactive()
{
    InterlockedExchange(&m_runFlag, 0);
    if (m_allocator != nullptr) {
        m_allocator->Decommit(); // unblocks GetBuffer
    }
    if (m_thread != nullptr) {
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    return S_OK;
}

void CameraPin::shutdown()
{
    Inactive();
    if (m_connected != nullptr) {
        m_connected->Release();
        m_connected = nullptr;
    }
    if (m_allocator != nullptr) {
        m_allocator->Release();
        m_allocator = nullptr;
    }
    freeMediaType(&m_mt);
    ZeroMemory(&m_mt, sizeof(AM_MEDIA_TYPE));
}

DWORD WINAPI CameraPin::threadEntry(LPVOID param)
{
    static_cast<CameraPin*>(param)->threadLoop();
    return 0;
}

bool CameraPin::fillSample(uint8_t* buffer, size_t capacity,
                           uint64_t frameIndex)
{
    uint32_t width = 0, height = 0, stride = 0;
    if (m_reader.readLatest(buffer, capacity, &width, &height, &stride) &&
        width == m_format.width && height == m_format.height) {
        return true;
    }
    // Controller not running or resolution changed underneath us: serve
    // the test pattern so the camera never looks dead.
    drawTestPattern(buffer, m_format.width, m_format.height, frameIndex);
    return true;
}

void CameraPin::threadLoop()
{
    const int64_t perFrame100ns =
        10000000i64 / (m_format.fps ? m_format.fps : 30);
    const int64_t frequency = qpcFrequency();
    const int64_t perFrameQpc = frequency / (m_format.fps ? m_format.fps : 30);

    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
                                          CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
    if (timer == nullptr) {
        timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }

    int64_t next = qpcNow();
    while (InterlockedCompareExchange(&m_runFlag, 0, 0) != 0) {
        if (m_connected == nullptr || m_allocator == nullptr) {
            break;
        }
        IMediaSample* sample = nullptr;
        if (FAILED(m_allocator->GetBuffer(&sample, nullptr, nullptr, 0))) {
            break; // decommitted while stopping
        }
        BYTE* data = nullptr;
        if (SUCCEEDED(sample->GetPointer(&data)) && data != nullptr) {
            fillSample(data, static_cast<size_t>(sample->GetSize()),
                       m_frameIndex);
            sample->SetActualDataLength(
                static_cast<LONG>(m_format.width * m_format.height * 2));
        }
        const REFERENCE_TIME tStart =
            static_cast<REFERENCE_TIME>(m_frameIndex * perFrame100ns);
        const REFERENCE_TIME tEnd = tStart + perFrame100ns;
        sample->SetTime(&tStart, &tEnd);
        sample->SetSyncPoint(TRUE);
        sample->SetDiscontinuity(m_frameIndex == 0);

        const HRESULT hr = m_connected->Receive(sample);
        sample->Release();
        if (hr == VFW_E_WRONG_STATE) {
            break; // graph is being torn down
        }

        ++m_frameIndex;

        // Pace to the configured frame rate.
        next += perFrameQpc;
        const int64_t now = qpcNow();
        if (next > now && timer != nullptr) {
            LARGE_INTEGER due{};
            due.QuadPart = -(next - now);
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, INFINITE);
        } else if (next < now - perFrameQpc) {
            next = now; // too far behind: do not burst
        }
    }
    if (timer != nullptr) {
        CloseHandle(timer);
    }
}

} // namespace vcamdriver

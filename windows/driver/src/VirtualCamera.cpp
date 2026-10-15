#include "VirtualCamera.h"

#include <cstring>

namespace vcamdriver {

namespace {

int dibStride(int width)
{
    return (width * 3 + 3) & ~3;
}

// YUYV (the shared memory format) -> RGB24 packed DIB rows.
void yuyvToRgb24(const uint8_t* yuyv, uint8_t* rgb, int width, int height,
                 int stride)
{
    for (int y = 0; y < height; ++y) {
        const uint8_t* src = yuyv + static_cast<size_t>(y) * width * 2;
        uint8_t* dst = rgb + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; x += 2) {
            const int y0 = src[0], u = src[1] - 128, y1 = src[2], v = src[3] - 128;
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
}

// Moving color bars in RGB24 (served when the controller is not running).
void drawTestPatternRgb24(uint8_t* rgb, int width, int height, int stride,
                          int frameIndex)
{
    static const uint8_t kBars[8][3] = {
        {192, 192, 192}, {192, 192, 0}, {0, 192, 192}, {0, 192, 0},
        {192, 0, 192},   {192, 0, 0},   {0, 0, 192},   {16, 16, 16},
    };
    const int barsBottom = height * 3 / 4;
    const int markerX = width > 0 ? frameIndex % width : 0;
    for (int y = 0; y < height; ++y) {
        uint8_t* row = rgb + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const uint8_t* color =
                y >= barsBottom ? kBars[7] : kBars[(x * 8 / (width ? width : 1)) % 8];
            uint8_t* pixel = row + x * 3;
            pixel[0] = color[0];
            pixel[1] = color[1];
            pixel[2] = color[2];
        }
        if (y < barsBottom) {
            for (int dx = 0; dx < 8 && markerX + dx < width; ++dx) {
                uint8_t* pixel = row + (markerX + dx) * 3;
                pixel[0] = pixel[1] = pixel[2] = 255;
            }
        }
    }
}

// Allocates a standalone RGB24 media type (CoTaskMem based, like softcam).
AM_MEDIA_TYPE* makeMediaType(int width, int height, int fps)
{
    if (fps <= 0) {
        fps = 30;
    }
    const int stride = dibStride(width);
    const int size = stride * height;

    BYTE* format = static_cast<BYTE*>(CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
    if (format == nullptr) {
        return nullptr;
    }
    AM_MEDIA_TYPE* mt =
        static_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    if (mt == nullptr) {
        CoTaskMemFree(format);
        return nullptr;
    }
    memset(mt, 0, sizeof(*mt));

    auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(format);
    memset(vih, 0, sizeof(*vih));
    vih->dwBitRate = static_cast<uint32_t>(
        static_cast<int64_t>(width) * height * 24 * fps);
    vih->AvgTimePerFrame = 10000000i64 / fps;
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth = width;
    vih->bmiHeader.biHeight = height;
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 24;
    vih->bmiHeader.biCompression = BI_RGB;
    vih->bmiHeader.biSizeImage = size;

    mt->majortype = MEDIATYPE_Video;
    mt->subtype = MEDIASUBTYPE_RGB24;
    mt->bFixedSizeSamples = TRUE;
    mt->bTemporalCompression = FALSE;
    mt->lSampleSize = size;
    mt->formattype = FORMAT_VideoInfo;
    mt->pUnk = nullptr;
    mt->cbFormat = sizeof(VIDEOINFOHEADER);
    mt->pbFormat = format;
    return mt;
}

} // namespace

// ===== VirtualCamera (the filter) ==========================================

VirtualCamera::VirtualCamera(LPUNKNOWN lpUnk, HRESULT* phr)
    : CSource(NAME(L"Virtual Camera"), lpUnk, CLSID_VirtualCameraFilter, phr)
{
    VCAM_TRACE_P("VirtualCamera ctor enter");
    m_width = static_cast<int>(vcam::kDefaultWidth);
    m_height = static_cast<int>(vcam::kDefaultHeight);
    m_fps = static_cast<int>(vcam::kDefaultFps);

    // Adopt the controller's current configuration when it is running.
    SharedFrameReader reader;
    uint32_t width = 0, height = 0, fps = 0;
    if (reader.readHeader(&width, &height, &fps)) {
        m_width = static_cast<int>(width);
        m_height = static_cast<int>(height);
        if (fps > 0) {
            m_fps = static_cast<int>(fps);
        }
    }

    // The stream registers itself with this filter (CSource::AddPin).
    VCAM_TRACE_P("VirtualCamera ctor creating stream");
    (void)new VirtualCameraStream(phr, this, L"Output");
    VCAM_TRACE_P("VirtualCamera ctor done");
}

CUnknown* WINAPI VirtualCamera::CreateInstance(LPUNKNOWN lpUnk, HRESULT* phr)
{
    VCAM_TRACE_P("VirtualCamera::CreateInstance enter");
    auto* filter = new (std::nothrow) VirtualCamera(lpUnk, phr);
    if (filter == nullptr && phr != nullptr) {
        *phr = E_OUTOFMEMORY;
    }
    return filter;
}

STDMETHODIMP VirtualCamera::NonDelegatingQueryInterface(REFIID riid,
                                                        void** ppv)
{
    char msg[128];
    sprintf_s(msg, "VirtualCamera::NQI riid=%08lX ppv=%p", riid.Data1, ppv);
    VCAM_TRACE_P(msg);
    if (riid == IID_IAMStreamConfig) {
        VCAM_TRACE_P("VirtualCamera::NQI -> IAMStreamConfig path");
        return GetInterface(static_cast<IAMStreamConfig*>(this), ppv);
    }
    VCAM_TRACE_P("VirtualCamera::NQI -> CSource base path");
    HRESULT hr = CSource::NonDelegatingQueryInterface(riid, ppv);
    VCAM_TRACE_P("VirtualCamera::NQI -> base done");
    return hr;
}

STDMETHODIMP VirtualCamera::SetFormat(AM_MEDIA_TYPE* mt)
{
    if (mt == nullptr) {
        return E_POINTER;
    }
    if (mt->majortype != MEDIATYPE_Video || mt->subtype != MEDIASUBTYPE_RGB24) {
        return E_FAIL;
    }
    if (mt->formattype == FORMAT_VideoInfo && mt->pbFormat != nullptr) {
        const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(mt->pbFormat);
        if (vih->bmiHeader.biWidth != m_width ||
            vih->bmiHeader.biHeight != m_height ||
            vih->bmiHeader.biBitCount != 24 ||
            vih->bmiHeader.biCompression != BI_RGB) {
            return E_FAIL;
        }
    }
    return S_OK;
}

STDMETHODIMP VirtualCamera::GetFormat(AM_MEDIA_TYPE** outPmt)
{
    if (outPmt == nullptr) {
        return E_POINTER;
    }
    *outPmt = makeMediaType(m_width, m_height, m_fps);
    return *outPmt != nullptr ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP VirtualCamera::GetNumberOfCapabilities(int* outCount,
                                                    int* outSize)
{
    if (outCount == nullptr || outSize == nullptr) {
        return E_POINTER;
    }
    *outCount = 1;
    *outSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

STDMETHODIMP VirtualCamera::GetStreamCaps(int index, AM_MEDIA_TYPE** outPmt,
                                          BYTE* outScc)
{
    if (outPmt == nullptr || outScc == nullptr) {
        return E_POINTER;
    }
    if (index >= 1) {
        return S_FALSE;
    }
    AM_MEDIA_TYPE* mt = makeMediaType(m_width, m_height, m_fps);
    if (mt == nullptr) {
        return E_OUTOFMEMORY;
    }
    *outPmt = mt;
    memset(outScc, 0, sizeof(VIDEO_STREAM_CONFIG_CAPS));
    const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(mt->pbFormat);
    auto* scc = reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(outScc);
    scc->guid = FORMAT_VideoInfo;
    scc->InputSize = SIZE{m_width, m_height};
    scc->MinCroppingSize = SIZE{m_width, m_height};
    scc->MaxCroppingSize = SIZE{m_width, m_height};
    scc->CropGranularityX = 1;
    scc->CropGranularityY = 1;
    scc->CropAlignX = 1;
    scc->CropAlignY = 1;
    scc->MinOutputSize = SIZE{m_width, m_height};
    scc->MaxOutputSize = SIZE{m_width, m_height};
    scc->OutputGranularityX = 1;
    scc->OutputGranularityY = 1;
    scc->MinFrameInterval = vih->AvgTimePerFrame;
    scc->MaxFrameInterval = vih->AvgTimePerFrame;
    scc->MinBitsPerSecond = static_cast<LONG>(vih->dwBitRate);
    scc->MaxBitsPerSecond = static_cast<LONG>(vih->dwBitRate);
    return S_OK;
}

// ===== VirtualCameraStream (the output pin) ================================

VirtualCameraStream::VirtualCameraStream(HRESULT* phr, VirtualCamera* parent,
                                         LPCWSTR pinName)
    : CSourceStream(NAME(L"Virtual Camera Stream"), phr, parent, pinName)
{
    VCAM_TRACE_P("Stream ctor enter");
}

VirtualCameraStream::~VirtualCameraStream() = default;

VirtualCamera* VirtualCameraStream::parent() const
{
    return static_cast<VirtualCamera*>(m_pFilter);
}

STDMETHODIMP VirtualCameraStream::NonDelegatingQueryInterface(REFIID riid,
                                                              void** ppv)
{
    VCAM_TRACE_P("Stream::NonDelegatingQI enter");
    if (riid == IID_IKsPropertySet) {
        return GetInterface(static_cast<IKsPropertySet*>(this), ppv);
    }
    if (riid == IID_IAMStreamConfig) {
        return GetInterface(static_cast<IAMStreamConfig*>(this), ppv);
    }
    return CSourceStream::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT VirtualCameraStream::GetMediaType(CMediaType* pmt)
{
    if (pmt == nullptr) {
        return E_POINTER;
    }
    VIDEOINFOHEADER* vih = reinterpret_cast<VIDEOINFOHEADER*>(
        pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    if (vih == nullptr) {
        return E_OUTOFMEMORY;
    }
    ZeroMemory(vih, sizeof(VIDEOINFOHEADER));

    const int width = parent()->width();
    const int height = parent()->height();
    const int fps = parent()->fps();
    const int stride = dibStride(width);

    vih->dwBitRate =
        static_cast<uint32_t>(static_cast<int64_t>(width) * height * 24 * fps);
    vih->AvgTimePerFrame = 10000000i64 / (fps > 0 ? fps : 30);
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth = width;
    vih->bmiHeader.biHeight = height;
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 24;
    vih->bmiHeader.biCompression = BI_RGB;
    vih->bmiHeader.biSizeImage = stride * height;

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetSubtype(&MEDIASUBTYPE_RGB24);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);
    pmt->SetSampleSize(stride * height);
    return NOERROR;
}

HRESULT VirtualCameraStream::CheckMediaType(const CMediaType* pmt)
{
    VCAM_TRACE_P("Stream::CheckMediaType enter");
    if (pmt == nullptr) {
        return E_POINTER;
    }
    // Accept our single RGB24 format by field (the default base classes
    // comparison is byte-exact and rejects equivalent types whose padding
    // or informational fields differ).
    if (pmt->majortype != MEDIATYPE_Video ||
        pmt->subtype != MEDIASUBTYPE_RGB24 ||
        pmt->formattype != FORMAT_VideoInfo ||
        pmt->cbFormat < sizeof(VIDEOINFOHEADER) || pmt->pbFormat == nullptr) {
        return S_FALSE;
    }
    const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(pmt->pbFormat);
    if (vih->bmiHeader.biWidth != parent()->width() ||
        vih->bmiHeader.biHeight != parent()->height() ||
        vih->bmiHeader.biBitCount != 24 ||
        vih->bmiHeader.biCompression != BI_RGB) {
        return S_FALSE;
    }
    return S_OK;
}

HRESULT VirtualCameraStream::DecideBufferSize(IMemAllocator* alloc,
                                              ALLOCATOR_PROPERTIES* props)
{
    CheckPointer(alloc, E_POINTER);
    CheckPointer(props, E_POINTER);

    CAutoLock lock(m_pFilter->pStateLock());
    const int stride = dibStride(parent()->width());
    props->cBuffers = 1;
    props->cbBuffer = stride * parent()->height();
    props->cbAlign = 1;
    props->cbPrefix = 0;

    ALLOCATOR_PROPERTIES actual{};
    HRESULT hr = alloc->SetProperties(props, &actual);
    if (FAILED(hr)) {
        return hr;
    }
    if (actual.cbBuffer < props->cbBuffer) {
        return E_FAIL;
    }
    return NOERROR;
}

HRESULT VirtualCameraStream::OnThreadCreate()
{
    const int fps = parent()->fps();
    m_interval = 10000000i64 / (fps > 0 ? fps : 30);
    m_sampleTime = 0;
    m_frameIndex = 0;
    return NOERROR;
}

HRESULT VirtualCameraStream::deliverSharedFrame(uint8_t* rgb, int width,
                                                int height, int stride,
                                                int /*frameIndex*/)
{
    const size_t frameBytes = static_cast<size_t>(width) * height * 2;
    if (m_yuyv.size() < frameBytes) {
        m_yuyv.resize(frameBytes);
    }
    uint32_t sw = 0, sh = 0, sstride = 0;
    if (!m_reader.readLatest(m_yuyv.data(), m_yuyv.size(), &sw, &sh, &sstride) ||
        static_cast<int>(sw) != width || static_cast<int>(sh) != height) {
        return S_FALSE; // controller not running or resolution changed
    }
    yuyvToRgb24(m_yuyv.data(), rgb, width, height, stride);
    return S_OK;
}

HRESULT VirtualCameraStream::deliverTestPattern(uint8_t* rgb, int width,
                                                int height, int stride,
                                                int frameIndex)
{
    drawTestPatternRgb24(rgb, width, height, stride, frameIndex);
    return S_OK;
}

HRESULT VirtualCameraStream::FillBuffer(IMediaSample* sample)
{
    CheckPointer(sample, E_POINTER);

    BYTE* data = nullptr;
    sample->GetPointer(&data);
    if (data == nullptr) {
        return E_POINTER;
    }

    const int width = parent()->width();
    const int height = parent()->height();
    const int stride = dibStride(width);
    const long size = static_cast<long>(stride) * height;
    if (sample->GetSize() < size) {
        return E_FAIL;
    }

    if (deliverSharedFrame(data, width, height, stride, m_frameIndex) != S_OK) {
        deliverTestPattern(data, width, height, stride, m_frameIndex);
    }
    ++m_frameIndex;

    sample->SetActualDataLength(size);
    sample->SetSyncPoint(TRUE);
    sample->SetDiscontinuity(FALSE);
    sample->SetPreroll(FALSE);

    REFERENCE_TIME start = m_sampleTime;
    m_sampleTime += m_interval;
    sample->SetTime(&start, &m_sampleTime);
    return NOERROR;
}

STDMETHODIMP VirtualCameraStream::SetFormat(AM_MEDIA_TYPE* mt)
{
    return parent()->SetFormat(mt);
}

STDMETHODIMP VirtualCameraStream::GetFormat(AM_MEDIA_TYPE** outPmt)
{
    return parent()->GetFormat(outPmt);
}

STDMETHODIMP VirtualCameraStream::GetNumberOfCapabilities(int* outCount,
                                                          int* outSize)
{
    return parent()->GetNumberOfCapabilities(outCount, outSize);
}

STDMETHODIMP VirtualCameraStream::GetStreamCaps(int index,
                                                AM_MEDIA_TYPE** outPmt,
                                                BYTE* outScc)
{
    return parent()->GetStreamCaps(index, outPmt, outScc);
}

// --- IKsPropertySet (mirrors the proven softcam implementation) -------------

STDMETHODIMP VirtualCameraStream::Get(REFGUID guidPropSet, DWORD dwPropID,
                                      LPVOID, DWORD, LPVOID propData,
                                      DWORD cbPropData, DWORD* pcbReturned)
{
    VCAM_TRACE_P("Stream::Get enter");
    if (guidPropSet != AMPROPSETID_Pin) {
        return E_PROP_SET_UNSUPPORTED;
    }
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) {
        return E_PROP_ID_UNSUPPORTED;
    }
    if (propData == nullptr && pcbReturned == nullptr) {
        return E_POINTER;
    }
    if (pcbReturned != nullptr) {
        *pcbReturned = sizeof(GUID);
    }
    if (propData != nullptr) {
        if (cbPropData < sizeof(GUID)) {
            return E_UNEXPECTED;
        }
        *static_cast<GUID*>(propData) = PIN_CATEGORY_CAPTURE;
    }
    return S_OK;
}

STDMETHODIMP VirtualCameraStream::Set(REFGUID, DWORD, LPVOID, DWORD, LPVOID,
                                      DWORD)
{
    return E_NOTIMPL;
}

STDMETHODIMP VirtualCameraStream::QuerySupported(REFGUID guidPropSet,
                                                 DWORD dwPropID,
                                                 DWORD* pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) {
        return E_PROP_SET_UNSUPPORTED;
    }
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) {
        return E_PROP_ID_UNSUPPORTED;
    }
    if (pTypeSupport != nullptr) {
        *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    }
    return S_OK;
}

} // namespace vcamdriver

// VirtualCamera.h - the DirectShow capture source filter, built on the
// standard DirectShow base classes (the proven approach used by softcam and
// the classic vcam sample: CSource + CSourceStream handle all pin
// negotiation, allocator setup and streaming; we only provide the media
// type, the frame data and the IKsPropertySet pin-category query that
// browsers use to find the capture pin).

#pragma once

// IMPORTANT: <streams.h> first. The base classes' reftime.h declares
// identifiers (MILLISECONDS, ...) that ks.h/ksmedia.h redefine as macros
// when included earlier.
#include <streams.h>

#include "Common.h"
#include "SharedFrameReader.h"

#include <cstdint>
#include <vector>

namespace vcamdriver {

class VirtualCamera;

// The single output pin. Serves RGB24 (BI_RGB) frames like softcam: the
// controller's YUYV shared memory frames are converted per frame.
class VirtualCameraStream : public CSourceStream,
                            public IAMStreamConfig,
                            public IKsPropertySet
{
public:
    VirtualCameraStream(HRESULT* phr, VirtualCamera* parent, LPCWSTR pinName);
    ~VirtualCameraStream() override;

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

protected:
    // CSourceStream
    HRESULT FillBuffer(IMediaSample* sample) override;
    HRESULT GetMediaType(CMediaType* pmt) override; // single-format source
    HRESULT CheckMediaType(const CMediaType* pmt) override;
    HRESULT DecideBufferSize(IMemAllocator* alloc,
                             ALLOCATOR_PROPERTIES* props) override;
    HRESULT OnThreadCreate() override;

    // IAMStreamConfig (delegated to the filter, like softcam)
    STDMETHODIMP SetFormat(AM_MEDIA_TYPE* mt) override;
    STDMETHODIMP GetFormat(AM_MEDIA_TYPE** outPmt) override;
    STDMETHODIMP GetNumberOfCapabilities(int* outCount, int* outSize) override;
    STDMETHODIMP GetStreamCaps(int index, AM_MEDIA_TYPE** outPmt,
                               BYTE* outScc) override;

    // IKsPropertySet - browsers and the capture graph builder discover the
    // capture pin through AMPROPERTY_PIN_CATEGORY.
    STDMETHODIMP Get(REFGUID guidPropSet, DWORD dwPropID, LPVOID instanceData,
                     DWORD cbInstanceData, LPVOID propData, DWORD cbPropData,
                     DWORD* pcbReturned) override;
    STDMETHODIMP Set(REFGUID guidPropSet, DWORD dwPropID, LPVOID instanceData,
                     DWORD cbInstanceData, LPVOID propData,
                     DWORD cbPropData) override;
    STDMETHODIMP QuerySupported(REFGUID guidPropSet, DWORD dwPropID,
                                DWORD* pTypeSupport) override;

private:
    VirtualCamera* parent() const;
    HRESULT deliverSharedFrame(uint8_t* rgb, int width, int height, int stride,
                               int frameIndex);
    HRESULT deliverTestPattern(uint8_t* rgb, int width, int height, int stride,
                               int frameIndex);

    SharedFrameReader m_reader;
    std::vector<uint8_t> m_yuyv; // staging for the shared memory frame
    REFERENCE_TIME m_sampleTime = 0;
    REFERENCE_TIME m_interval = 333333; // 100ns units (30 fps)
    int m_frameIndex = 0;
    ULONGLONG m_lastDeliverTick = 0; // pacing (GetTickCount64, ms)
};

class VirtualCamera : public CSource, public IAMStreamConfig
{
public:
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN lpUnk, HRESULT* phr);

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;

    // IAMStreamConfig (filter level, exposed like softcam)
    STDMETHODIMP SetFormat(AM_MEDIA_TYPE* mt) override;
    STDMETHODIMP GetFormat(AM_MEDIA_TYPE** outPmt) override;
    STDMETHODIMP GetNumberOfCapabilities(int* outCount, int* outSize) override;
    STDMETHODIMP GetStreamCaps(int index, AM_MEDIA_TYPE** outPmt,
                               BYTE* outScc) override;

    int width() const { return m_width; }
    int height() const { return m_height; }
    int fps() const { return m_fps; }

private:
    VirtualCamera(LPUNKNOWN lpUnk, HRESULT* phr);

    int m_width = 0;
    int m_height = 0;
    int m_fps = 30;
};

} // namespace vcamdriver

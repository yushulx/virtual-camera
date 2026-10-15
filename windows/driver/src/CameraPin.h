// CameraPin.h - the capture output pin: IPin + IAMStreamConfig plus the
// frame delivery thread.

#pragma once

#include "Common.h"
#include "MediaTypes.h"
#include "SharedFrameReader.h"

namespace vcamdriver {

class CameraFilter;
class MemAllocator;

class CameraPin : public IPin, public IAMStreamConfig
{
public:
    explicit CameraPin(CameraFilter* filter);
    ~CameraPin() override;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IPin
    STDMETHOD(Connect)(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(ReceiveConnection)(IPin* pConnector,
                                 const AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(Disconnect)() override;
    STDMETHOD(ConnectedTo)(IPin** pPin) override;
    STDMETHOD(ConnectionMediaType)(AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(QueryPinInfo)(PIN_INFO* pInfo) override;
    STDMETHOD(QueryDirection)(PIN_DIRECTION* pPinDir) override;
    STDMETHOD(QueryId)(LPWSTR* Id) override;
    STDMETHOD(QueryAccept)(const AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(EnumMediaTypes)(IEnumMediaTypes** ppEnum) override;
    STDMETHOD(QueryInternalConnections)(IPin** apPin, ULONG* nPin) override;
    STDMETHOD(BeginFlush)() override;
    STDMETHOD(EndFlush)() override;
    STDMETHOD(EndOfStream)() override;

    // IAMStreamConfig
    STDMETHOD(SetFormat)(AM_MEDIA_TYPE* pmt) override;
    STDMETHOD(GetFormat)(AM_MEDIA_TYPE** ppmt) override;
    STDMETHOD(GetNumberOfCapabilities)(int* piCount, int* piSize) override;
    STDMETHOD(GetStreamCaps)(int iIndex, BYTE* pSCC,
                              AM_MEDIA_TYPE** ppmt) override;

    // --- internal API (called by the filter / enumerators) ----------------

    // Start the delivery thread (Pause or Run).
    HRESULT Active();
    // Stop the delivery thread (Stop).
    HRESULT Inactive();
    // Final teardown, called from the filter destructor.
    void shutdown();

    // Build the current media type; caller frees it with freeMediaType().
    bool currentMediaType(AM_MEDIA_TYPE* mt);

    bool isConnected() const { return m_connected != nullptr; }

private:
    static DWORD WINAPI threadEntry(LPVOID param);
    void threadLoop();
    bool fillSample(uint8_t* buffer, size_t capacity, uint64_t frameIndex);

    // The format the controller currently advertises (shared memory header
    // when present, defaults otherwise).
    CameraFormat currentDesiredFormat() const;

    LONG m_refs = 1;
    CameraFilter* m_filter;         // owning reference
    IPin* m_connected = nullptr;   // downstream input pin (AddRef'd)
    MemAllocator* m_allocator = nullptr;
    AM_MEDIA_TYPE m_mt{};          // connected media type (deep owned)
    CameraFormat m_format;         // format in use
    SharedFrameReader m_reader;
    HANDLE m_thread = nullptr;
    volatile LONG m_runFlag = 0;
    uint64_t m_frameIndex = 0;
};

} // namespace vcamdriver

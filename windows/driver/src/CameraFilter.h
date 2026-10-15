// CameraFilter.h - the IBaseFilter implementation registered in the
// DirectShow "Video Input Devices" category.

#pragma once

#include "Common.h"

namespace vcamdriver {

class CameraPin;

class CameraFilter : public IBaseFilter
{
public:
    CameraFilter();

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IPersist
    STDMETHOD(GetClassID)(CLSID* pClassID) override;

    // IMediaFilter
    STDMETHOD(Stop)() override;
    STDMETHOD(Pause)() override;
    STDMETHOD(Run)(REFERENCE_TIME tStart) override;
    STDMETHOD(GetState)(DWORD dwMilliSecs, FILTER_STATE* State) override;
    STDMETHOD(SetSyncSource)(IReferenceClock* pClock) override;
    STDMETHOD(GetSyncSource)(IReferenceClock** pClock) override;

    // IBaseFilter
    STDMETHOD(EnumPins)(IEnumPins** ppEnum) override;
    STDMETHOD(FindPin)(LPCWSTR Id, IPin** ppPin) override;
    STDMETHOD(QueryFilterInfo)(FILTER_INFO* pInfo) override;
    STDMETHOD(JoinFilterGraph)(IFilterGraph* pGraph, LPCWSTR pName) override;
    STDMETHOD(QueryVendorInfo)(LPWSTR* pVendorInfo) override;

private:
    ~CameraFilter(); // released through Release()

    LONG m_refs = 1;
    FILTER_STATE m_state = State_Stopped;
    IFilterGraph* m_graph = nullptr;     // AddRef'd, may be null
    IReferenceClock* m_clock = nullptr; // AddRef'd, may be null
    CameraPin* m_pin = nullptr;          // owned, holds a reference on us
    wchar_t m_name[80] = L"Virtual Camera";
};

} // namespace vcamdriver

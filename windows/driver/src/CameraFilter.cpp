#include "CameraFilter.h"

#include "CameraPin.h"
#include "EnumPins.h"

#include <new>

namespace vcamdriver {

CameraFilter::CameraFilter()
{
    m_pin = new (std::nothrow) CameraPin(this);
}

CameraFilter::~CameraFilter()
{
    if (m_pin != nullptr) {
        m_pin->shutdown();
        m_pin->Release();
        m_pin = nullptr;
    }
    if (m_graph != nullptr) {
        m_graph->Release();
    }
    if (m_clock != nullptr) {
        m_clock->Release();
    }
}

// --- IUnknown ----------------------------------------------------------------

STDMETHODIMP CameraFilter::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IPersist ||
        riid == IID_IMediaFilter || riid == IID_IBaseFilter) {
        *ppv = static_cast<IBaseFilter*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CameraFilter::AddRef()
{
    return InterlockedIncrement(&m_refs);
}

STDMETHODIMP_(ULONG) CameraFilter::Release()
{
    const ULONG refs = InterlockedDecrement(&m_refs);
    if (refs == 0) {
        delete this;
    }
    return refs;
}

// --- IPersist -----------------------------------------------------------------

STDMETHODIMP CameraFilter::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr) {
        return E_POINTER;
    }
    *pClassID = CLSID_VirtualCameraFilter;
    return S_OK;
}

// --- IMediaFilter -------------------------------------------------------------

STDMETHODIMP CameraFilter::Stop()
{
    m_state = State_Stopped;
    if (m_pin != nullptr) {
        m_pin->Inactive();
    }
    return S_OK;
}

STDMETHODIMP CameraFilter::Pause()
{
    // Like other live sources we keep delivering while paused so consumers
    // can complete their preroll.
    m_state = State_Paused;
    if (m_pin != nullptr) {
        m_pin->Active();
    }
    return S_OK;
}

STDMETHODIMP CameraFilter::Run(REFERENCE_TIME /*tStart*/)
{
    m_state = State_Running;
    if (m_pin != nullptr) {
        m_pin->Active();
    }
    return S_OK;
}

STDMETHODIMP CameraFilter::GetState(DWORD, FILTER_STATE* State)
{
    if (State == nullptr) {
        return E_POINTER;
    }
    *State = m_state;
    return S_OK;
}

STDMETHODIMP CameraFilter::SetSyncSource(IReferenceClock* pClock)
{
    if (pClock != nullptr) {
        pClock->AddRef();
    }
    if (m_clock != nullptr) {
        m_clock->Release();
    }
    m_clock = pClock;
    return S_OK;
}

STDMETHODIMP CameraFilter::GetSyncSource(IReferenceClock** pClock)
{
    if (pClock == nullptr) {
        return E_POINTER;
    }
    if (m_clock == nullptr) {
        *pClock = nullptr;
        return S_FALSE;
    }
    *pClock = m_clock;
    (*pClock)->AddRef();
    return S_OK;
}

// --- IBaseFilter ---------------------------------------------------------------

STDMETHODIMP CameraFilter::EnumPins(IEnumPins** ppEnum)
{
    if (ppEnum == nullptr) {
        return E_POINTER;
    }
    if (m_pin == nullptr) {
        return E_OUTOFMEMORY;
    }
    auto* enumerator = new (std::nothrow) EnumPins(m_pin);
    if (enumerator == nullptr) {
        return E_OUTOFMEMORY;
    }
    *ppEnum = enumerator;
    return S_OK;
}

STDMETHODIMP CameraFilter::FindPin(LPCWSTR Id, IPin** ppPin)
{
    if (Id == nullptr || ppPin == nullptr) {
        return E_POINTER;
    }
    if (wcscmp(Id, L"1") == 0 && m_pin != nullptr) {
        m_pin->AddRef();
        *ppPin = static_cast<IPin*>(m_pin);
        return S_OK;
    }
    *ppPin = nullptr;
    return VFW_E_NOT_FOUND;
}

STDMETHODIMP CameraFilter::QueryFilterInfo(FILTER_INFO* pInfo)
{
    if (pInfo == nullptr) {
        return E_POINTER;
    }
    wchar_t* name = static_cast<wchar_t*>(
        CoTaskMemAlloc(sizeof(wchar_t) * (wcslen(m_name) + 1)));
    if (name == nullptr) {
        return E_OUTOFMEMORY;
    }
    wcscpy_s(name, wcslen(m_name) + 1, m_name);
    pInfo->achName = name;
    pInfo->pGraph = m_graph;
    if (pInfo->pGraph != nullptr) {
        pInfo->pGraph->AddRef();
    }
    return S_OK;
}

STDMETHODIMP CameraFilter::JoinFilterGraph(IFilterGraph* pGraph,
                                           LPCWSTR pName)
{
    if (m_graph != nullptr) {
        m_graph->Release();
        m_graph = nullptr;
    }
    m_graph = pGraph;
    if (m_graph != nullptr) {
        m_graph->AddRef();
    }
    if (pName != nullptr) {
        wcsncpy_s(m_name, pName, _TRUNCATE);
    } else {
        wcscpy_s(m_name, kCameraName);
    }
    return S_OK;
}

STDMETHODIMP CameraFilter::QueryVendorInfo(LPWSTR* pVendorInfo)
{
    if (pVendorInfo == nullptr) {
        return E_POINTER;
    }
    return E_NOTIMPL;
}

} // namespace vcamdriver

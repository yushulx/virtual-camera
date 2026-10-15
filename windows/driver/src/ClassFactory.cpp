#include "ClassFactory.h"

#include "CameraFilter.h"

#include <new>

namespace vcamdriver {

ClassFactory::~ClassFactory() = default;

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ClassFactory::AddRef()
{
    return InterlockedIncrement(&m_refs);
}

STDMETHODIMP_(ULONG) ClassFactory::Release()
{
    const ULONG refs = InterlockedDecrement(&m_refs);
    if (refs == 0) {
        delete this;
    }
    return refs;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                         void** ppvObject)
{
    if (ppvObject == nullptr) {
        return E_POINTER;
    }
    if (pUnkOuter != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }
    auto* filter = new (std::nothrow) CameraFilter();
    if (filter == nullptr) {
        *ppvObject = nullptr;
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = filter->QueryInterface(riid, ppvObject);
    filter->Release();
    return hr;
}

STDMETHODIMP ClassFactory::LockServer(BOOL /*fLock*/)
{
    return S_OK; // the DLL never unloads (DllCanUnloadNow returns S_FALSE)
}

} // namespace vcamdriver

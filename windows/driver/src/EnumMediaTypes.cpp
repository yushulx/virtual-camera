#include "EnumMediaTypes.h"

#include "CameraPin.h"
#include "MediaTypes.h"

#include <new>

namespace vcamdriver {

EnumMediaTypes::EnumMediaTypes(CameraPin* pin)
    : m_pin(pin)
{
    m_pin->AddRef();
}

EnumMediaTypes::~EnumMediaTypes()
{
    m_pin->Release();
}

STDMETHODIMP EnumMediaTypes::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IEnumMediaTypes) {
        *ppv = static_cast<IEnumMediaTypes*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EnumMediaTypes::AddRef()
{
    return InterlockedIncrement(&m_refs);
}

STDMETHODIMP_(ULONG) EnumMediaTypes::Release()
{
    const ULONG refs = InterlockedDecrement(&m_refs);
    if (refs == 0) {
        delete this;
    }
    return refs;
}

STDMETHODIMP EnumMediaTypes::Next(ULONG cMediaTypes,
                                  AM_MEDIA_TYPE** ppMediaTypes,
                                  ULONG* pcFetched)
{
    if (ppMediaTypes == nullptr || (cMediaTypes > 1 && pcFetched == nullptr)) {
        return E_POINTER;
    }
    ULONG fetched = 0;
    while (fetched < cMediaTypes) {
        AM_MEDIA_TYPE mt{};
        if (!m_pin->currentMediaType(&mt)) {
            break;
        }
        ppMediaTypes[fetched] = static_cast<AM_MEDIA_TYPE*>(
            CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
        if (ppMediaTypes[fetched] == nullptr) {
            freeMediaType(&mt);
            break;
        }
        *ppMediaTypes[fetched] = mt; // take ownership of pbFormat
        ++fetched;
    }
    if (pcFetched) {
        *pcFetched = fetched;
    }
    return fetched == cMediaTypes ? S_OK : S_FALSE;
}

STDMETHODIMP EnumMediaTypes::Skip(ULONG cMediaTypes)
{
    // One format only.
    m_position += cMediaTypes;
    return m_position <= 1 ? S_OK : S_FALSE;
}

STDMETHODIMP EnumMediaTypes::Reset()
{
    m_position = 0;
    return S_OK;
}

STDMETHODIMP EnumMediaTypes::Clone(IEnumMediaTypes** ppEnum)
{
    if (ppEnum == nullptr) {
        return E_POINTER;
    }
    auto* clone = new (std::nothrow) EnumMediaTypes(m_pin);
    if (clone == nullptr) {
        return E_OUTOFMEMORY;
    }
    clone->m_position = m_position;
    *ppEnum = clone;
    return S_OK;
}

} // namespace vcamdriver

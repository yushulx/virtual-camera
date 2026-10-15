#include "EnumPins.h"

#include "CameraPin.h"

#include <new>

namespace vcamdriver {

EnumPins::EnumPins(CameraPin* pin)
    : m_pin(pin)
{
    m_pin->AddRef();
}

EnumPins::~EnumPins()
{
    m_pin->Release();
}

STDMETHODIMP EnumPins::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IEnumPins) {
        *ppv = static_cast<IEnumPins*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EnumPins::AddRef()
{
    return InterlockedIncrement(&m_refs);
}

STDMETHODIMP_(ULONG) EnumPins::Release()
{
    const ULONG refs = InterlockedDecrement(&m_refs);
    if (refs == 0) {
        delete this;
    }
    return refs;
}

STDMETHODIMP EnumPins::Next(ULONG cPins, IPin** ppPins, ULONG* pcFetched)
{
    if (ppPins == nullptr || (cPins > 1 && pcFetched == nullptr)) {
        return E_POINTER;
    }
    ULONG fetched = 0;
    while (fetched < cPins && m_position == 0) {
        m_pin->AddRef();
        ppPins[fetched] = static_cast<IPin*>(m_pin);
        ++fetched;
        ++m_position;
    }
    if (pcFetched) {
        *pcFetched = fetched;
    }
    return fetched == cPins ? S_OK : S_FALSE;
}

STDMETHODIMP EnumPins::Skip(ULONG cPins)
{
    m_position += cPins;
    return m_position <= 1 ? S_OK : S_FALSE;
}

STDMETHODIMP EnumPins::Reset()
{
    m_position = 0;
    return S_OK;
}

STDMETHODIMP EnumPins::Clone(IEnumPins** ppEnum)
{
    if (ppEnum == nullptr) {
        return E_POINTER;
    }
    auto* clone = new (std::nothrow) EnumPins(m_pin);
    if (clone == nullptr) {
        return E_OUTOFMEMORY;
    }
    clone->m_position = m_position;
    *ppEnum = clone;
    return S_OK;
}

} // namespace vcamdriver

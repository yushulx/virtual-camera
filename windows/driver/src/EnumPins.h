// EnumPins.h - IEnumPins over the single output pin.

#pragma once

#include "Common.h"

namespace vcamdriver {

class CameraPin;

class EnumPins : public IEnumPins
{
public:
    explicit EnumPins(CameraPin* pin);

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IEnumPins
    STDMETHOD(Next)(ULONG cPins, IPin** ppPins, ULONG* pcFetched) override;
    STDMETHOD(Skip)(ULONG cPins) override;
    STDMETHOD(Reset)() override;
    STDMETHOD(Clone)(IEnumPins** ppEnum) override;

private:
    ~EnumPins(); // released through Release()

    LONG m_refs = 1;
    CameraPin* m_pin; // owning reference
    ULONG m_position = 0;
};

} // namespace vcamdriver

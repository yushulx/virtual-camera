// EnumMediaTypes.h - IEnumMediaTypes over the single capture media type.

#pragma once

#include "Common.h"

namespace vcamdriver {

class CameraPin;

class EnumMediaTypes : public IEnumMediaTypes
{
public:
    explicit EnumMediaTypes(CameraPin* pin);

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IEnumMediaTypes
    STDMETHOD(Next)(ULONG cMediaTypes, AM_MEDIA_TYPE** ppMediaTypes,
                    ULONG* pcFetched) override;
    STDMETHOD(Skip)(ULONG cMediaTypes) override;
    STDMETHOD(Reset)() override;
    STDMETHOD(Clone)(IEnumMediaTypes** ppEnum) override;

private:
    ~EnumMediaTypes(); // released through Release()

    LONG m_refs = 1;
    CameraPin* m_pin; // owning reference
    ULONG m_position = 0;
};

} // namespace vcamdriver

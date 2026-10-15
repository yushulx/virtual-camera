// ClassFactory.h - IClassFactory for CLSID_VirtualCameraFilter.

#pragma once

#include "Common.h"

namespace vcamdriver {

class ClassFactory : public IClassFactory
{
public:
    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IClassFactory
    STDMETHOD(CreateInstance)(IUnknown* pUnkOuter, REFIID riid,
                              void** ppvObject) override;
    STDMETHOD(LockServer)(BOOL fLock) override;

private:
    ~ClassFactory(); // released through Release()

    LONG m_refs = 1;
};

} // namespace vcamdriver

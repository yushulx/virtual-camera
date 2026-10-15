// MemAllocator.h - minimal IMemAllocator implementation backed by one
// contiguous buffer plus a semaphore for GetBuffer blocking.

#pragma once

#include "MediaSample.h"

#include <vector>

namespace vcamdriver {

class MemAllocator : public IMemAllocator
{
public:
    MemAllocator() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IMemAllocator
    STDMETHOD(SetProperties)(ALLOCATOR_PROPERTIES* pRequest,
                              ALLOCATOR_PROPERTIES* pActual) override;
    STDMETHOD(GetProperties)(ALLOCATOR_PROPERTIES* pProps) override;
    STDMETHOD(Commit)() override;
    STDMETHOD(Decommit)() override;
    STDMETHOD(GetBuffer)(IMediaSample** ppBuffer, REFERENCE_TIME* pStartTime,
                         REFERENCE_TIME* pEndTime, DWORD dwFlags) override;
    STDMETHOD(ReleaseBuffer)(IMediaSample* pBuffer) override;
    STDMETHOD(SetNotify)(IMemAllocatorNotifyCallbackTemp* pNotify) override;
    STDMETHOD(GetFreeCount)(LONG* plBuffers) override;

    // Called by MediaSample when its refcount drops to zero.
    void releaseSample(MediaSample* sample);

private:
    ~MemAllocator(); // released through Release()

    LONG m_refs = 1;
    ALLOCATOR_PROPERTIES m_request{};
    bool m_committed = false;
    HANDLE m_semaphore = nullptr;
    uint8_t* m_buffer = nullptr;
    std::vector<MediaSample*> m_samples;
    CRITICAL_SECTION m_lock;
    bool m_lockInitialized = false;
};

} // namespace vcamdriver

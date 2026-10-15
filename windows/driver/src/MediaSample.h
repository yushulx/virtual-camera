// MediaSample.h - minimal IMediaSample implementation (a buffer handed to
// the consumer with timing information). Owned by MemAllocator.

#pragma once

#include "Common.h"

namespace vcamdriver {

class MemAllocator;

class MediaSample : public IMediaSample
{
    friend class MemAllocator; // owns the pool and deletes samples

public:
    MediaSample(MemAllocator* owner, uint8_t* buffer, long bufferSize);

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IMediaSample
    STDMETHOD(GetPointer)(BYTE** ppBuffer) override;
    STDMETHOD_(long, GetSize)() override;
    STDMETHOD(GetTime)(REFERENCE_TIME* pTimeStart,
                       REFERENCE_TIME* pTimeEnd) override;
    STDMETHOD(SetTime)(REFERENCE_TIME* pTimeStart,
                       REFERENCE_TIME* pTimeEnd) override;
    STDMETHOD(IsSyncPoint)() override;
    STDMETHOD(SetSyncPoint)(BOOL bIsSyncPoint) override;
    STDMETHOD(IsPreroll)() override;
    STDMETHOD(SetPreroll)(BOOL bIsPreroll) override;
    STDMETHOD_(long, GetActualDataLength)() override;
    STDMETHOD(SetActualDataLength)(long length) override;
    STDMETHOD(GetMediaType)(AM_MEDIA_TYPE** ppMediaType) override;
    STDMETHOD(SetMediaType)(AM_MEDIA_TYPE* pMediaType) override;
    STDMETHOD(IsDiscontinuity)() override;
    STDMETHOD(SetDiscontinuity)(BOOL bDiscontinuity) override;
    STDMETHOD(GetMediaTime)(LONGLONG* pTimeStart, LONGLONG* pTimeEnd) override;
    STDMETHOD(SetMediaTime)(LONGLONG* pTimeStart, LONGLONG* pTimeEnd) override;

    // Pool state (managed by the allocator; called under allocator control).
    bool inUse() const { return m_inUse; }
    void setInUse(bool used) { m_inUse = used; }

private:
    ~MediaSample(); // released through Release()

    MemAllocator* m_owner; // non-owning
    uint8_t* m_buffer;
    long m_bufferSize;
    long m_actualLength = 0;
    REFERENCE_TIME m_start = 0;
    REFERENCE_TIME m_end = 0;
    bool m_hasTime = false;
    bool m_syncPoint = false;
    bool m_preroll = false;
    bool m_discontinuity = false;
    bool m_inUse = false;
    LONG m_refs = 0; // 0 while parked in the pool
    AM_MEDIA_TYPE m_mediaType{}; // optional per-sample media type
};

} // namespace vcamdriver

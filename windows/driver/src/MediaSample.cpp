#include "MediaSample.h"

#include "MemAllocator.h"
#include "MediaTypes.h"

namespace vcamdriver {

MediaSample::MediaSample(MemAllocator* owner, uint8_t* buffer, long bufferSize)
    : m_owner(owner), m_buffer(buffer), m_bufferSize(bufferSize)
{
}

MediaSample::~MediaSample() = default;

STDMETHODIMP MediaSample::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IMediaSample) {
        *ppv = static_cast<IMediaSample*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) MediaSample::AddRef()
{
    return InterlockedIncrement(&m_refs);
}

STDMETHODIMP_(ULONG) MediaSample::Release()
{
    const ULONG refs = InterlockedDecrement(&m_refs);
    if (refs == 0) {
        // Parked back in the allocator pool; the object stays alive.
        m_owner->releaseSample(this);
    }
    return refs;
}

STDMETHODIMP MediaSample::GetPointer(BYTE** ppBuffer)
{
    if (ppBuffer == nullptr) {
        return E_POINTER;
    }
    *ppBuffer = m_buffer;
    return S_OK;
}

STDMETHODIMP_(long) MediaSample::GetSize()
{
    return m_bufferSize;
}

STDMETHODIMP MediaSample::GetTime(REFERENCE_TIME* pTimeStart,
                                  REFERENCE_TIME* pTimeEnd)
{
    if (pTimeStart == nullptr || pTimeEnd == nullptr) {
        return E_POINTER;
    }
    if (!m_hasTime) {
        return VFW_E_MEDIA_TIME_NOT_SET;
    }
    *pTimeStart = m_start;
    *pTimeEnd = m_end;
    return NOERROR;
}

STDMETHODIMP MediaSample::SetTime(REFERENCE_TIME* pTimeStart,
                                  REFERENCE_TIME* pTimeEnd)
{
    if (pTimeStart == nullptr || pTimeEnd == nullptr) {
        m_hasTime = false;
        return NOERROR;
    }
    m_start = *pTimeStart;
    m_end = *pTimeEnd;
    m_hasTime = true;
    return NOERROR;
}

STDMETHODIMP MediaSample::IsSyncPoint()
{
    return m_syncPoint ? S_OK : S_FALSE;
}

STDMETHODIMP MediaSample::SetSyncPoint(BOOL bIsSyncPoint)
{
    m_syncPoint = bIsSyncPoint != FALSE;
    return NOERROR;
}

STDMETHODIMP MediaSample::IsPreroll()
{
    return m_preroll ? S_OK : S_FALSE;
}

STDMETHODIMP MediaSample::SetPreroll(BOOL bIsPreroll)
{
    m_preroll = bIsPreroll != FALSE;
    return NOERROR;
}

STDMETHODIMP_(long) MediaSample::GetActualDataLength()
{
    return m_actualLength;
}

STDMETHODIMP MediaSample::SetActualDataLength(long length)
{
    if (length < 0 || length > m_bufferSize) {
        return E_INVALIDARG;
    }
    m_actualLength = length;
    return NOERROR;
}

STDMETHODIMP MediaSample::GetMediaType(AM_MEDIA_TYPE** ppMediaType)
{
    if (ppMediaType == nullptr) {
        return E_POINTER;
    }
    if (m_mediaType.pbFormat == nullptr) {
        *ppMediaType = nullptr;
        return S_FALSE;
    }
    return copyMediaType(*ppMediaType, &m_mediaType) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP MediaSample::SetMediaType(AM_MEDIA_TYPE* pMediaType)
{
    if (pMediaType == nullptr) {
        freeMediaType(&m_mediaType);
        ZeroMemory(&m_mediaType, sizeof(AM_MEDIA_TYPE));
        return S_OK;
    }
    freeMediaType(&m_mediaType);
    ZeroMemory(&m_mediaType, sizeof(AM_MEDIA_TYPE));
    return copyMediaType(&m_mediaType, pMediaType) ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP MediaSample::IsDiscontinuity()
{
    return m_discontinuity ? S_OK : S_FALSE;
}

STDMETHODIMP MediaSample::SetDiscontinuity(BOOL bDiscontinuity)
{
    m_discontinuity = bDiscontinuity != FALSE;
    return NOERROR;
}

STDMETHODIMP MediaSample::GetMediaTime(LONGLONG* pTimeStart,
                                       LONGLONG* pTimeEnd)
{
    if (pTimeStart == nullptr || pTimeEnd == nullptr) {
        return E_POINTER;
    }
    return VFW_E_MEDIA_TIME_NOT_SET;
}

STDMETHODIMP MediaSample::SetMediaTime(LONGLONG* /*pTimeStart*/,
                                       LONGLONG* /*pTimeEnd*/)
{
    return NOERROR;
}

} // namespace vcamdriver

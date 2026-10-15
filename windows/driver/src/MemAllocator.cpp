#include "MemAllocator.h"

namespace vcamdriver {

MemAllocator::~MemAllocator()
{
    if (m_semaphore) {
        CloseHandle(m_semaphore);
    }
    for (MediaSample* sample : m_samples) {
        delete sample;
    }
    if (m_buffer) {
        VirtualFree(m_buffer, 0, MEM_RELEASE);
    }
    if (m_lockInitialized) {
        DeleteCriticalSection(&m_lock);
    }
}

STDMETHODIMP MemAllocator::QueryInterface(REFIID riid, void** ppv)
{
    if (ppv == nullptr) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == IID_IMemAllocator) {
        *ppv = static_cast<IMemAllocator*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) MemAllocator::AddRef()
{
    return InterlockedIncrement(&m_refs);
}

STDMETHODIMP_(ULONG) MemAllocator::Release()
{
    const ULONG refs = InterlockedDecrement(&m_refs);
    if (refs == 0) {
        delete this;
    }
    return refs;
}

STDMETHODIMP MemAllocator::SetProperties(ALLOCATOR_PROPERTIES* pRequest,
                                         ALLOCATOR_PROPERTIES* pActual)
{
    if (pRequest == nullptr || pActual == nullptr) {
        return E_POINTER;
    }
    if (m_committed) {
        return VFW_E_ALREADY_COMMITTED;
    }
    // Sensible clamps; we are the source and know the frame size.
    m_request.cBuffers = pRequest->cBuffers > 0 ? pRequest->cBuffers : 3;
    m_request.cbBuffer = pRequest->cbBuffer;
    m_request.cbAlign = pRequest->cbAlign > 1 ? pRequest->cbAlign : 1;
    m_request.cbPrefix = 0; // not supported
    *pActual = m_request;
    return S_OK;
}

STDMETHODIMP MemAllocator::GetProperties(ALLOCATOR_PROPERTIES* pProps)
{
    if (pProps == nullptr) {
        return E_POINTER;
    }
    *pProps = m_request;
    return S_OK;
}

STDMETHODIMP MemAllocator::Commit()
{
    if (m_request.cbBuffer <= 0) {
        return VFW_E_SIZENOTSET;
    }
    if (m_committed) {
        return S_FALSE;
    }
    if (!m_lockInitialized) {
        InitializeCriticalSection(&m_lock);
        m_lockInitialized = true;
    }
    // Fresh semaphore (also handles re-commit after Decommit).
    if (m_semaphore) {
        CloseHandle(m_semaphore);
        m_semaphore = nullptr;
    }
    m_semaphore = CreateSemaphoreW(nullptr,
                                    static_cast<LONG>(m_request.cBuffers),
                                    static_cast<LONG>(m_request.cBuffers), nullptr);
    if (m_semaphore == nullptr) {
        return E_OUTOFMEMORY;
    }
    if (m_buffer == nullptr) {
        // First commit: allocate the pool.
        const SIZE_T total = static_cast<SIZE_T>(m_request.cBuffers) *
                             (m_request.cbBuffer + m_request.cbAlign);
        m_buffer = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, total, MEM_COMMIT | MEM_RESERVE,
                         PAGE_READWRITE));
        if (m_buffer == nullptr) {
            CloseHandle(m_semaphore);
            m_semaphore = nullptr;
            return E_OUTOFMEMORY;
        }
        for (int i = 0; i < m_request.cBuffers; ++i) {
            uint8_t* slot =
                m_buffer +
                static_cast<size_t>(i) * (m_request.cbBuffer + m_request.cbAlign);
            slot = reinterpret_cast<uint8_t*>(
                (reinterpret_cast<uintptr_t>(slot) + m_request.cbAlign - 1) /
                m_request.cbAlign * m_request.cbAlign);
            m_samples.push_back(new MediaSample(this, slot, m_request.cbBuffer));
        }
    } else {
        // Re-commit after Decommit: all samples are free again.
        EnterCriticalSection(&m_lock);
        for (MediaSample* sample : m_samples) {
            sample->setInUse(false);
        }
        LeaveCriticalSection(&m_lock);
    }
    m_committed = true;
    return S_OK;
}

STDMETHODIMP MemAllocator::Decommit()
{
    if (!m_committed) {
        return S_FALSE;
    }
    m_committed = false;
    // Unblock GetBuffer waiters; they will observe !m_committed and fail.
    for (int i = 0; i < m_request.cBuffers; ++i) {
        ReleaseSemaphore(m_semaphore, 1, nullptr);
    }
    return S_OK;
}

STDMETHODIMP MemAllocator::GetBuffer(IMediaSample** ppBuffer,
                                     REFERENCE_TIME* /*pStartTime*/,
                                     REFERENCE_TIME* /*pEndTime*/,
                                     DWORD /*dwFlags*/)
{
    if (ppBuffer == nullptr) {
        return E_POINTER;
    }
    *ppBuffer = nullptr;
    if (!m_committed) {
        return VFW_E_NOT_COMMITTED;
    }
    if (WaitForSingleObject(m_semaphore, INFINITE) != WAIT_OBJECT_0) {
        return E_FAIL;
    }
    if (!m_committed) {
        return VFW_E_NOT_COMMITTED; // released while waiting
    }
    EnterCriticalSection(&m_lock);
    MediaSample* sample = nullptr;
    for (MediaSample* candidate : m_samples) {
        if (!candidate->inUse()) {
            sample = candidate;
            break;
        }
    }
    if (sample == nullptr) {
        LeaveCriticalSection(&m_lock);
        return E_FAIL; // semaphore/samples out of sync
    }
    sample->setInUse(true);
    LeaveCriticalSection(&m_lock);
    sample->AddRef();
    *ppBuffer = sample;
    return S_OK;
}

STDMETHODIMP MemAllocator::ReleaseBuffer(IMediaSample* /*pBuffer*/)
{
    return S_OK; // samples return through releaseSample()
}

void MemAllocator::releaseSample(MediaSample* sample)
{
    EnterCriticalSection(&m_lock);
    sample->setInUse(false);
    LeaveCriticalSection(&m_lock);
    if (m_semaphore) {
        ReleaseSemaphore(m_semaphore, 1, nullptr);
    }
}

STDMETHODIMP MemAllocator::SetNotify(
    IMemAllocatorNotifyCallbackTemp* /*pNotify*/)
{
    return E_NOTIMPL; // optional interface member
}

STDMETHODIMP MemAllocator::GetFreeCount(LONG* plBuffers)
{
    if (plBuffers == nullptr) {
        return E_POINTER;
    }
    LONG freeSamples = 0;
    EnterCriticalSection(&m_lock);
    for (MediaSample* sample : m_samples) {
        if (!sample->inUse()) {
            ++freeSamples;
        }
    }
    LeaveCriticalSection(&m_lock);
    *plBuffers = freeSamples;
    return S_OK;
}

} // namespace vcamdriver

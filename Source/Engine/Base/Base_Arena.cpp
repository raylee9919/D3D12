// Copyright Seong Woo Lee. All Rights Reserved.

void* arena::PushSize(u64 Size)
{
    // @Temporary
    if (m_Reserved == 0) {
        m_Reserved = 64*1024;
        m_Base = (u8*)VirtualAlloc(NULL, m_Reserved, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    }

    ASSERT( m_Committed + Size <= m_Reserved );
    u8* Base = m_Base + m_Committed;
    m_Committed += Size;
    return (void*)Base;
}

template<typename T>
T* arena::Push(u32 Count)
{
    T* Result = (T*)PushSize(Count*sizeof(T));
    return Result;
}

void arena::Clear(void)
{
    m_Committed = 0;
}

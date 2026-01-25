// Copyright Seong Woo Lee. All Rights Reserved.

class arena
{
    public:
        void* PushSize(u64 Size);
        template<typename T> T* Push(u32 Count);
        void Clear(void);
    private:
        u8* m_Base      = nullptr;
        u64 m_Reserved  = 0;
        u64 m_Committed = 0;
};

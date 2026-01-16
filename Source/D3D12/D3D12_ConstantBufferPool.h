// Copyright Seong Woo Lee. All Rights Reserved.

struct cbv_descriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {};
    BYTE *m_CPUMappedAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_GPUVirtualAddress = {};
};

struct constant_buffer_pool
{
    UINT m_DescriptorSize;
    UINT m_MaxCount;
    UINT m_Count = 0;
    ID3D12Resource *m_Resource = nullptr;
    ID3D12DescriptorHeap *m_Heap = nullptr;
    BYTE *m_CPUAddress = nullptr;
    cbv_descriptor *m_CBVArray = nullptr;

    void Init(UINT MaxCount, D3D12_DESCRIPTOR_HEAP_FLAGS Flags);
    cbv_descriptor* Alloc();
    void Clear();
};

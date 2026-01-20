// Copyright Seong Woo Lee. All Rights Reserved.

void constant_buffer_pool::Init(UINT MaxCount, D3D12_DESCRIPTOR_HEAP_FLAGS Flags)
{
    m_MaxCount = MaxCount;
    m_DescriptorSize = (sizeof(simple_constant_buffer) + 255) & ~255;

    const UINT Capacity = m_DescriptorSize*m_MaxCount;

    auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Capacity);
    ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_Resource)) );


    // Create descriptor heap.
    D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = MaxCount,
        .Flags = Flags
    };
    ASSERT_SUCCEEDED( d3d12->Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&m_Heap)) );
    CD3DX12_RANGE ReadRange(0, 0);
    m_Resource->Map(0, &ReadRange, (void **)&m_CPUAddress);
    BYTE *CPUPtr = m_CPUAddress;


    D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc = {
        .BufferLocation = m_Resource->GetGPUVirtualAddress(),
        .SizeInBytes    = m_DescriptorSize
    };

    CD3DX12_CPU_DESCRIPTOR_HANDLE Handle(m_Heap->GetCPUDescriptorHandleForHeapStart());

    UINT DescriptorSize = d3d12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_CBVArray = new cbv_descriptor[MaxCount];
    for (UINT i = 0 ; i < MaxCount; ++i)
    {
        d3d12->Device->CreateConstantBufferView(&CBVDesc, Handle);

        cbv_descriptor *Descriptor = m_CBVArray + i;
        Descriptor->m_CPUHandle         = Handle;
        Descriptor->m_GPUVirtualAddress = CBVDesc.BufferLocation;
        Descriptor->m_CPUMappedAddress  = CPUPtr;

        Handle.Offset(1, DescriptorSize);
        CBVDesc.BufferLocation += m_DescriptorSize;
        CPUPtr += m_DescriptorSize;
    }
}

cbv_descriptor* constant_buffer_pool::Alloc()
{
    ASSERT( m_Count < m_MaxCount );
    cbv_descriptor* Result = m_CBVArray + m_Count;
    ++m_Count;
    return Result;
}

void constant_buffer_pool::Clear()
{
    m_Count = 0;
}

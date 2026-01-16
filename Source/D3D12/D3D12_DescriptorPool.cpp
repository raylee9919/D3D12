// Copyright Seong Woo Lee. All Rights Reserved.

void descriptor_pool::Init(UINT MaxCount, D3D12_DESCRIPTOR_HEAP_FLAGS Flags)
{
    m_MaxCount = MaxCount;
    m_DescriptorSize = d3d12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC CommonHeapDesc = {
        .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = MaxCount,
        .Flags          = Flags
    };

    ASSERT_SUCCEEDED( d3d12->Device->CreateDescriptorHeap(&CommonHeapDesc, IID_PPV_ARGS(&m_Heap)) );
    m_CPUHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
    if (Flags == D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
    {
    }
    else if (Flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        m_GPUHandle = m_Heap->GetGPUDescriptorHandleForHeapStart();
    }
    else
    {
        ASSERT( !"Unknown flags." );
    }
}

void descriptor_pool::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE *OutCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE *OutGPUHandle, u32 Count)
{
    const UINT NewCount = m_Count + Count;
    ASSERT( NewCount <= m_MaxCount );

    if (OutCPUHandle)
    {
        *OutCPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_CPUHandle, m_Count, m_DescriptorSize);
    }
    if (OutGPUHandle)
    {
        *OutGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_GPUHandle, m_Count, m_DescriptorSize);
    }

    m_Count = NewCount;
}

void descriptor_pool::Clear()
{
    m_Count = 0;
}

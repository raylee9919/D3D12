// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)


Descriptor_Pool::Descriptor_Pool(ID3D12Device5 *Device, u32 Capacity) {
    m_Device = Device;
    m_Capacity = Capacity;
    m_DescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
    {
        Desc.NumDescriptors = m_Capacity;
        Desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        Desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    }

    w32Assert(m_Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&m_DescriptorHeap)));

    m_DescriptorCPUHandle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_DescriptorGPUHandle = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}


Descriptor_Pool::~Descriptor_Pool() {

}

void Descriptor_Pool::AllocDescriptors(u32 Count, D3D12_CPU_DESCRIPTOR_HANDLE *OutCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE *OutGPUHandle) {
    Assert(m_Count + Count <= m_Capacity);

    *OutCPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_DescriptorCPUHandle, m_Count, m_DescriptorSize);
    *OutGPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_DescriptorGPUHandle, m_Count, m_DescriptorSize);

    m_Count += Count;
}

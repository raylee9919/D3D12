// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

class Descriptor_Pool {
    public:
        Descriptor_Pool(ID3D12Device5 *Device, u32 Capacity);
        ~Descriptor_Pool();

        void AllocDescriptors(u32 Count, D3D12_CPU_DESCRIPTOR_HANDLE *OutCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE *OutGPUHandle);

    private:
        ID3D12Device5 *m_Device = nullptr;
        u32 m_Count = 0;
        u32 m_Capacity = 0;
        u32 m_DescriptorSize = 0;
        ID3D12DescriptorHeap *m_DescriptorHeap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE	m_DescriptorCPUHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE	m_DescriptorGPUHandle = {};
};

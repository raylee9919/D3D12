// Copyright Seong Woo Lee. All Rights Reserved.

#pragma once

struct descriptor_pool
{
    UINT m_DescriptorSize;
    UINT m_MaxCount;
    UINT m_Count = 0;
    ID3D12DescriptorHeap* m_Heap = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE	m_CPUHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE	m_GPUHandle = {};

    void Init(UINT MaxCount, D3D12_DESCRIPTOR_HEAP_FLAGS Flags);
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE *OutCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE *OutGpuHandle, u32 Count);
    void Clear();
};

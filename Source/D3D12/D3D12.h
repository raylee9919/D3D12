// Copyright Seong Woo Lee. All Rights Reserved.

#include "D3D12_ConstantBufferPool.h"
#include "D3D12_DescriptorPool.h"

#define MAX_FRAME_COUNT 3

struct D3D12_Fence 
{
    ID3D12Fence *ptr;
    HANDLE event;
    UINT64 value;
};

struct texture
{
    ID3D12Resource *m_Resource = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {};
};

struct D3D12_State 
{
    IDXGIFactory4 *dxgi_factory;
    ID3D12Device5 *Device;
    DXGI_ADAPTER_DESC1 adapter_desc;
    IDXGISwapChain3 *swap_chain;

    ID3D12Resource *render_target_views[MAX_FRAME_COUNT];
    UINT frame_count;
    UINT frame_index;

    ID3D12CommandAllocator    *CommandAllocator;
    ID3D12CommandQueue        *CommandQueue;
    ID3D12GraphicsCommandList *CommandList;

    D3D12_Fence m_Fence;

    ID3D12DescriptorHeap *rtv_heap;
    UINT rtv_descriptor_size;

    ID3D12DescriptorHeap *m_DSVHeap;
    UINT m_DSVDescriptorSize;
    ID3D12Resource *m_DepthStencilResource;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;

    descriptor_pool *m_DescriptorPool;
    descriptor_pool *m_SRVDescriptorPool;

    constant_buffer_pool *m_ConstantBufferPool;

    // Camera
    M4x4 m_View = M4x4Identity();
    M4x4 m_Proj = M4x4Identity();

    void End();
};

// Functions
//
static D3D12_Fence d3d12CreateFence();

// Globals
//
static D3D12_State *d3d12;

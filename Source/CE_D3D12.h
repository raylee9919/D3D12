/* ========================================================================
   File: %
   Date: %
   Revision: %
   Creator: Seong Woo Lee
   Notice: (C) Copyright % by Seong Woo Lee. All Rights Reserved.
   ======================================================================== */

#define MAX_FRAME_COUNT 3

struct D3D12_Fence {
    ID3D12Fence *ptr;
    HANDLE event;
    UINT64 value;
};

struct D3D12_State {
    IDXGIFactory4 *dxgi_factory;
    ID3D12Device5 *device;
    DXGI_ADAPTER_DESC1 adapter_desc;
    IDXGISwapChain3 *swap_chain;

    ID3D12Resource *render_target_views[MAX_FRAME_COUNT];
    UINT frame_count;
    UINT frame_index;

    ID3D12CommandAllocator    *command_allocator;
    ID3D12CommandQueue        *command_queue;
    ID3D12GraphicsCommandList *command_list;

    D3D12_Fence fence;

    ID3D12DescriptorHeap *rtv_heap;
    UINT rtv_descriptor_size;

    CD3DX12_VIEWPORT viewport;
    CD3DX12_RECT scissor_rect;

    ID3D12RootSignature *root_signature;
    ID3D12PipelineState *pipeline_state;
};


// Functions
//
Internal D3D12_Fence d3d12CreateFence();

// Globals
//
Global D3D12_State *d3d12;

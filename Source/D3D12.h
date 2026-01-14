// Copyright Seong Woo Lee. All Rights Reserved.

#define MAX_FRAME_COUNT 3

struct D3D12_Fence 
{
    ID3D12Fence *ptr;
    HANDLE event;
    UINT64 value;
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

    D3D12_Fence fence;

    ID3D12DescriptorHeap *rtv_heap;
    UINT rtv_descriptor_size;

    CD3DX12_VIEWPORT Viewport;
    CD3DX12_RECT     ScissorRect;
};

struct descriptor_pool
{
    UINT MaxCount;
    UINT DescriptorSize;
    ID3D12DescriptorHeap *Heap;
	D3D12_CPU_DESCRIPTOR_HANDLE	CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE	GPUHandle;
};

struct cbv_descriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE Handle;
    BYTE *CPUAddress;
    D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;
};

struct constant_buffer_pool
{
    UINT MaxCount;
    UINT CBVSize;
    ID3D12Resource *Resource;
    ID3D12DescriptorHeap *CBVHeap;
    BYTE *CPUAddress;
    cbv_descriptor *CBVArray;
};


// Functions
//
Internal D3D12_Fence d3d12CreateFence();

// Globals
//
Global D3D12_State *d3d12;

// Copyright Seong Woo Lee. All Rights Reserved.

#pragma once
#include "D3D12_DescriptorPool.h"
#include "D3D12_ResourceManager.h"
#include "D3D12_CommandListPool.h"
#include "D3D12_ConstantBufferPool.h"
#include "D3D12_MeshObject.h"
#include "D3D12_RenderQueue.h"

#define SWAPCHAIN_FRAME_COUNT 3
#define MAX_PENDING_FRAME_COUNT ((SWAPCHAIN_FRAME_COUNT)-1)
#define RENDER_THREAD_COUNT 6

class d3d12_renderer : public i_renderer
{
    public:
        void Init(HWND Window, UINT Width, UINT Height);
        void UpdateFramebuffer(UINT Width, UINT Height);
        CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvHandle();
        CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHandle();
        UINT64 Fence();
        void FenceWait(UINT64 ExpectedValue);

        virtual i_mesh* CreateMesh(void* Vertices, u32 VertexSize, u32 VerticesCount, void* Indices, u32 IndexSize, u32 IndicesCount) override;




        IDXGIFactory4* dxgi_factory;
        ID3D12Device5* Device;
        DXGI_ADAPTER_DESC1 m_AdapterDesc;
        IDXGISwapChain3 *m_SwapChain;
        UINT m_SwapChainFlags;

        ID3D12Resource* m_RenderTargets[SWAPCHAIN_FRAME_COUNT];
        UINT m_RenderTargetIndex;

        resource_manager* m_ResourceManager = nullptr;

        ID3D12CommandQueue* m_CommandQueue;

        ID3D12Fence* m_Fence = nullptr;
        HANDLE m_FenceEvent = {};
        UINT64 m_LastFenceValues[MAX_PENDING_FRAME_COUNT] = {};
        UINT64 m_FenceValue = 0;

        UINT m_CurrentContextIndex = 0;
        LONG volatile m_RemainThreadCount = 0;
        HANDLE m_CompletionEvent = {};
        UINT m_CurrentThreadIndex = 0;
        constant_buffer_pool* m_ConstantBufferPools[MAX_PENDING_FRAME_COUNT][RENDER_THREAD_COUNT] = {};
        descriptor_pool *m_DescriptorPools[MAX_PENDING_FRAME_COUNT][RENDER_THREAD_COUNT] = {};
        command_list_pool* m_CommandListPools[MAX_PENDING_FRAME_COUNT][RENDER_THREAD_COUNT] = {};
        render_queue* m_RenderQueues[RENDER_THREAD_COUNT] = {};
        struct render_thread_context* m_RenderThreadContexts[RENDER_THREAD_COUNT] = {};

        ID3D12DescriptorHeap *m_RTVHeap;
        UINT m_RTVDescriptorSize;

        UINT m_DSVDescriptorSize;

        D3D12_VIEWPORT m_Viewport;
        D3D12_RECT     m_ScissorRect;




        // Camera
        M4x4 m_View = M4x4Identity();
        M4x4 m_Proj = M4x4Identity();

    private:
        ID3D12DescriptorHeap* m_DSVHeap = nullptr;
        ID3D12Resource *m_DepthStencilResource = nullptr;

        d3d12_mesh* m_LineMesh = nullptr;
        d3d12_mesh* CreateLineMesh();

        void CreateSwapChain(HWND hWindow, UINT BufferCount, UINT Width, UINT Height, ID3D12CommandQueue* CommandQueue);
        void CreateDepthStencil(UINT Width, UINT Height, ID3D12DescriptorHeap* DescriptorHeap, ID3D12Resource** OutResource);
};

struct render_thread_context
{
    d3d12_renderer* Renderer;
    UINT Index = 0;
    HANDLE Event = {};
};




extern "C" __declspec(dllexport) void InitRenderer(d3d12_renderer* Renderer, HWND Window, UINT Width, UINT Height);

static DWORD WINAPI RenderThreadProc(LPVOID Param);


// Globals
//
static d3d12_renderer* d3d12;

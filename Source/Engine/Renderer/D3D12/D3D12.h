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
#define RENDER_THREAD_COUNT 8

struct render_thread_context
{
    UINT Index = 0;
    HANDLE Event = {};
};

struct D3D12_State 
{
    UINT64 Fence();
    void FenceWait(UINT64 ExpectedValue);

    IDXGIFactory4 *dxgi_factory;
    ID3D12Device5 *Device;
    DXGI_ADAPTER_DESC1 adapter_desc;
    IDXGISwapChain3 *m_SwapChain;
    UINT m_SwapChainFlags;

    ID3D12Resource *m_RenderTargets[SWAPCHAIN_FRAME_COUNT];
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
    command_list_pool* _CommandListPools[MAX_PENDING_FRAME_COUNT][RENDER_THREAD_COUNT] = {};
    render_queue* m_RenderQueues[RENDER_THREAD_COUNT] = {};
    render_thread_context* m_RenderThreadContexts[RENDER_THREAD_COUNT] = {};

    ID3D12DescriptorHeap *m_RTVHeap;
    UINT m_RTVDescriptorSize;

    ID3D12DescriptorHeap *m_DSVHeap;
    UINT m_DSVDescriptorSize;
    ID3D12Resource *m_DepthStencilResource;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;




    // Camera
    M4x4 m_View = M4x4Identity();
    M4x4 m_Proj = M4x4Identity();
};


// Globals
//
static D3D12_State *d3d12;

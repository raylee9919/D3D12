// Copyright Seong Woo Lee. All Rights Reserved.

#include "D3D12_ResourceManager.cpp"
#include "D3D12_CommandListPool.cpp"
#include "D3D12_ConstantBufferPool.cpp"
#include "D3D12_DescriptorPool.cpp"
#include "D3D12_MeshObject.cpp"
#include "D3D12_RenderQueue.cpp"


static void d12CreateSwapChain(HWND hwnd, ID3D12CommandQueue* CommandQueue, UINT BufferCount, UINT Width, UINT Height) 
{
    DXGI_SWAP_CHAIN_DESC1 Desc = {
        .Width       = Width,
        .Height      = Height,
        .Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo      = 0,
        .SampleDesc = {
            .Count   = 1,
            .Quality = 0
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = BufferCount,
        .Scaling     = DXGI_SCALING_NONE,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode   = DXGI_ALPHA_MODE_IGNORE,
        .Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING,
    };
    d3d12->m_SwapChainFlags = Desc.Flags;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC FullScreenDesc = {
        .Windowed = TRUE
    };

    IDXGISwapChain1* SwapChain1 = nullptr;

    ASSERT_SUCCEEDED( d3d12->dxgi_factory->CreateSwapChainForHwnd(CommandQueue, hwnd, &Desc, &FullScreenDesc, nullptr, &SwapChain1) );

    SwapChain1->QueryInterface(IID_PPV_ARGS(&d3d12->m_SwapChain));
    SwapChain1->Release();
    d3d12->m_RenderTargetIndex = d3d12->m_SwapChain->GetCurrentBackBufferIndex();
}

static void d12CreateDepthStencil(UINT Width, UINT Height)
{
    { // Create DSV
        D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc = {
            .Format        = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags         = D3D12_DSV_FLAG_NONE,
        };

        D3D12_CLEAR_VALUE ClearValue = {
            .Format       = DXGI_FORMAT_D32_FLOAT,
            .DepthStencil = {
                .Depth   = 1.f,
                .Stencil = 0
            }
        };

        CD3DX12_RESOURCE_DESC DepthDesc(D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, Width, Height, 1, 1, DXGI_FORMAT_R32_TYPELESS, 1, 0,  D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        auto DefaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&DefaultHeapProp, D3D12_HEAP_FLAG_NONE, &DepthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue, IID_PPV_ARGS(&d3d12->m_DepthStencilResource)) );
        d3d12->m_DepthStencilResource->SetName(L"m_DepthStencilResource");

        CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle(d3d12->m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
        d3d12->Device->CreateDepthStencilView(d3d12->m_DepthStencilResource, &DSVDesc, DSVHandle);
    }
}

static void d12UpdateFramebuffer(UINT Width, UINT Height)
{
    ASSERT( Width!=0 || Height!=0 );

    d3d12->Fence();
    for (UINT i = 0; i < MAX_PENDING_FRAME_COUNT; ++i)
    {
        d3d12->FenceWait(d3d12->m_LastFenceValues[i]);
    }

    // Swapchain
    DXGI_SWAP_CHAIN_DESC1 Desc = {};
    ASSERT_SUCCEEDED( d3d12->m_SwapChain->GetDesc1(&Desc) );

    for (int i = 0; i < SWAPCHAIN_FRAME_COUNT; ++i)
    {
        d3d12->m_RenderTargets[i]->Release();
        d3d12->m_RenderTargets[i] = nullptr;
    }

    ASSERT_SUCCEEDED( d3d12->m_SwapChain->ResizeBuffers(SWAPCHAIN_FRAME_COUNT, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, d3d12->m_SwapChainFlags) );

    d3d12->m_RenderTargetIndex = d3d12->m_SwapChain->GetCurrentBackBufferIndex();

    CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12->m_RTVHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < SWAPCHAIN_FRAME_COUNT; ++i)
    {
        d3d12->m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12->m_RenderTargets[i]));
        d3d12->Device->CreateRenderTargetView(d3d12->m_RenderTargets[i], nullptr, RTVHandle);
        RTVHandle.Offset(1, d3d12->m_RTVDescriptorSize);
    }


    // Depth, Stencil
    if (d3d12->m_DepthStencilResource)
    {
        d3d12->m_DepthStencilResource->Release();
    }
    d12CreateDepthStencil(Width, Height);


    // Viewport and scissor rect.
    d3d12->m_Viewport.Width  = (FLOAT)Width;
    d3d12->m_Viewport.Height = (FLOAT)Height;
    d3d12->m_ScissorRect.left   = 0;
    d3d12->m_ScissorRect.top    = 0;
    d3d12->m_ScissorRect.right  = Width;
    d3d12->m_ScissorRect.bottom = Height;
}

DWORD WINAPI RenderThreadProc(LPVOID Param)
{
    render_thread_context* Context = (render_thread_context*)Param;
    const UINT ThreadIndex = Context->Index;
    for(;;)
    {
        WaitForSingleObject(Context->Event, INFINITE);

        command_list_pool* CommandListPool = d3d12->_CommandListPools[d3d12->m_CurrentContextIndex][ThreadIndex];
        descriptor_pool* DescriptorPool = d3d12->m_DescriptorPools[d3d12->m_CurrentContextIndex][ThreadIndex];
        constant_buffer_pool* ConstantBufferPool = d3d12->m_ConstantBufferPools[d3d12->m_CurrentContextIndex][ThreadIndex];
        d3d12->m_RenderQueues[ThreadIndex]->Process(CommandListPool, d3d12->m_CommandQueue, DescriptorPool, ConstantBufferPool, 400);

        LONG RemainThreadCount = InterlockedDecrement(&d3d12->m_RemainThreadCount);
        if (RemainThreadCount == 0) 
        {
            SetEvent(d3d12->m_CompletionEvent);
        }
    }

    //return 0;
}

UINT64 D3D12_State::Fence()
{
    m_FenceValue++;
    m_CommandQueue->Signal(m_Fence, m_FenceValue);
    m_LastFenceValues[m_CurrentContextIndex] = m_FenceValue;
    return m_FenceValue;
}

void D3D12_State::FenceWait(UINT64 ExpectedValue) 
{
    if (m_Fence->GetCompletedValue() < ExpectedValue) 
    {
        m_Fence->SetEventOnCompletion(ExpectedValue, m_FenceEvent);
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }
}

void d12Init(HWND hwnd) 
{
    IDXGIAdapter1 *Adapter = nullptr;
    DXGI_ADAPTER_DESC1 adapter_desc = {};

    ID3D12Debug *DebugController = nullptr;
    DWORD create_factory_flags = 0;

#if 1
    {
        // Enable Debug Layer
        if (SUCCEEDED( D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController)) )) 
        {
            DebugController->EnableDebugLayer();
            create_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        // GBV
        ID3D12Debug5 *DebugController5 = nullptr;
        if (SUCCEEDED( DebugController->QueryInterface(IID_PPV_ARGS(&DebugController5)) )) 
        {
            DebugController5->SetEnableGPUBasedValidation(TRUE);
            DebugController5->SetEnableAutoName(TRUE);
            DebugController5->Release();
        }
    }
#endif

    // Create DXGI-Factory
    //
    CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&d3d12->dxgi_factory));

    D3D_FEATURE_LEVEL FeatureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    UINT FeatureLevelCount = CountOf(FeatureLevels);

    for (UINT i = 0; i < FeatureLevelCount; ++i) 
    {
        int adapter_index = 0;
        while (d3d12->dxgi_factory->EnumAdapters1(adapter_index, &Adapter) != DXGI_ERROR_NOT_FOUND) 
        {
            Adapter->GetDesc1(&adapter_desc);

            if (SUCCEEDED(D3D12CreateDevice(Adapter, FeatureLevels[i], IID_PPV_ARGS(&d3d12->Device)))) 
            {
                goto lb_exit;
            }

            Adapter->Release();
            ++adapter_index;
        }
    }

lb_exit:
    ASSERT( d3d12->Device );
    d3d12->adapter_desc = adapter_desc;


#if 1
    if (DebugController) 
    {
        ID3D12InfoQueue *InfoQueue = nullptr;
        d3d12->Device->QueryInterface(IID_PPV_ARGS(&InfoQueue));

        ASSERT( InfoQueue );

        InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

		D3D12_MESSAGE_ID hide[] = {
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

        D3D12_INFO_QUEUE_FILTER Filter = {};
        {
            Filter.DenyList.NumIDs = (UINT)CountOf(hide);
            Filter.DenyList.pIDList = hide;
        }
        InfoQueue->AddStorageFilterEntries(&Filter);
        InfoQueue->Release();
    }
#endif

    D3D12_COMMAND_QUEUE_DESC Desc = {
        .Type  = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
    }; 
    ASSERT_SUCCEEDED( d3d12->Device->CreateCommandQueue(&Desc, IID_PPV_ARGS(&d3d12->m_CommandQueue)) );


    d3d12->m_ResourceManager = new resource_manager;
    d3d12->m_ResourceManager->Init(d3d12->Device);

    for (UINT i = 0; i < MAX_PENDING_FRAME_COUNT; ++i)
    {
        for (UINT j = 0; j < RENDER_THREAD_COUNT; ++j)
        {
            d3d12->_CommandListPools[i][j] = new command_list_pool;
            d3d12->_CommandListPools[i][j]->Init(d3d12->Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        }
    }


    { // Create swap chain.
        // @Temporary
        const UINT Width = 1920;
        const UINT Height = 1080;
        d12CreateSwapChain(hwnd, d3d12->m_CommandQueue, SWAPCHAIN_FRAME_COUNT, Width, Height);
    }


    { // Create RTV descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC Desc = {
            .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = SWAPCHAIN_FRAME_COUNT,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };
        d3d12->Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&d3d12->m_RTVHeap));
        d3d12->m_RTVDescriptorSize = d3d12->Device->GetDescriptorHandleIncrementSize(Desc.Type);
    }

    
    { // Each frame gets RTV.
        CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12->m_RTVHeap->GetCPUDescriptorHandleForHeapStart());

        for (UINT i = 0; i < SWAPCHAIN_FRAME_COUNT; ++i) 
        {
            ASSERT_SUCCEEDED( d3d12->m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12->m_RenderTargets[i])) );
            d3d12->Device->CreateRenderTargetView(d3d12->m_RenderTargets[i], nullptr, RTVHandle);
            RTVHandle.Offset(1, d3d12->m_RTVDescriptorSize);
        }
    }

    // Get Rect.
    RECT Rect = {};
    GetClientRect(hwnd, &Rect);
    DWORD Width  = Rect.right  - Rect.left;
    DWORD Height = Rect.bottom - Rect.top;


    { // Create DSV Heap
        D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc = {
            .Type  = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        };
        ASSERT_SUCCEEDED( d3d12->Device->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(&d3d12->m_DSVHeap)) );

        d3d12->m_DSVDescriptorSize = d3d12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }


    d12CreateDepthStencil(Width, Height);


    UINT64 InitialValue = 0;
    d3d12->Device->CreateFence(InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12->m_Fence));
    d3d12->m_FenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    d3d12->m_FenceValue = InitialValue;


    { // Set viewport, scissor rect.
        d3d12->m_Viewport = {
            .TopLeftX = 0.f,
            .TopLeftY = 0.f,
            .Width    = (FLOAT)Width,
            .Height   = (FLOAT)Height,
            .MinDepth = 0.f,
            .MaxDepth = 1.f,
        };

        d3d12->m_ScissorRect = {
            .left   = 0,
            .top    = 0,
            .right  = (LONG)Width,
            .bottom = (LONG)Height
        };
    }
    
    for (UINT i = 0; i < MAX_PENDING_FRAME_COUNT; ++i)
    {

        for (UINT j = 0; j < RENDER_THREAD_COUNT; ++j)
        {
            d3d12->m_ConstantBufferPools[i][j] = new constant_buffer_pool;
            d3d12->m_ConstantBufferPools[i][j]->Init(4096, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
        }
    }

    for (UINT i = 0; i < MAX_PENDING_FRAME_COUNT; ++i)
    {
        for (UINT j = 0; j < RENDER_THREAD_COUNT; ++j)
        {
            d3d12->m_DescriptorPools[i][j] = new descriptor_pool;
            d3d12->m_DescriptorPools[i][j]->Init(8192, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
        }
    }



    for (UINT i = 0; i < RENDER_THREAD_COUNT; ++i)
    {
        d3d12->m_RenderQueues[i] = new render_queue();
        d3d12->m_RenderQueues[i]->Init(4096);

        d3d12->m_RenderThreadContexts[i] = new render_thread_context;
        memset(d3d12->m_RenderThreadContexts[i], 0, sizeof(render_thread_context));
    }

    // Init render threads
    d3d12->m_CompletionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    for (UINT i = 0; i < RENDER_THREAD_COUNT; ++i)
    {
        render_thread_context* Context = d3d12->m_RenderThreadContexts[i];
        Context->Index = i;
        Context->Event = CreateEvent(NULL, FALSE, FALSE, NULL);

        HANDLE ThreadHandle = CreateThread(NULL, 0, RenderThreadProc, Context, 0, nullptr/*ThreadId*/);
        CloseHandle(ThreadHandle);
    }



    // Cleanup
    if (DebugController) 
    {
        DebugController->Release(); 
    }
    if (Adapter) 
    {
        Adapter->Release(); 
    }
}

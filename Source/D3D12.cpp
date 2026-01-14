// Copyright Seong Woo Lee. All Rights Reserved.

static D3D12_Fence d12CreateFence() 
{
    D3D12_Fence result = {};
    UINT64 initial_value = 0;
    d3d12->Device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&result.ptr));
    result.value = initial_value;
    result.event = CreateEventW(nullptr/*security attr*/, FALSE, FALSE, nullptr/*name*/);
    return result;
}

static void d12Fence(D3D12_Fence &fence) 
{
    fence.value += 1;
    d3d12->CommandQueue->Signal(fence.ptr, fence.value);
}

static void d12FenceWait(D3D12_Fence &fence) 
{
    UINT64 expected_value = fence.value;
    UINT64 current_value = fence.ptr->GetCompletedValue();

    // @Temporary
    if (current_value != expected_value) {
        fence.ptr->SetEventOnCompletion(expected_value, fence.event);
        WaitForSingleObject(fence.event, INFINITE);
    }
}

static ID3D12CommandQueue *d12CreateCommandQueue() 
{
    ID3D12CommandQueue *result = nullptr;
    D3D12_COMMAND_QUEUE_DESC desc = {}; 
    {
        desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    }

    ASSERT_SUCCEEDED(d3d12->Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&result)));
    return result;
}

static void d12CreateSwapChain(HWND hwnd, ID3D12CommandQueue *CommandQueue, UINT buffer_count, UINT width, UINT height) 
{
    DXGI_SWAP_CHAIN_DESC1 Desc = {
        .Width       = width,
        .Height      = height,
        .Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo      = 0,
        .SampleDesc = {
            .Count = 1,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = buffer_count,
        .Scaling     = DXGI_SCALING_NONE,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode   = DXGI_ALPHA_MODE_IGNORE,
        .Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING,
    };

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC full_screen_desc = {};
    {
        full_screen_desc.Windowed = TRUE;
    }

    IDXGISwapChain1 *swap_chain1 = nullptr;

    ASSERT_SUCCEEDED(d3d12->dxgi_factory->CreateSwapChainForHwnd(CommandQueue, hwnd, &Desc, &full_screen_desc, nullptr, &swap_chain1));

    swap_chain1->QueryInterface(IID_PPV_ARGS(&d3d12->swap_chain));
    swap_chain1->Release();
    d3d12->frame_index = d3d12->swap_chain->GetCurrentBackBufferIndex();
}

static void d12Present() 
{
    UINT sync_interval = 1; // 1: On, 0: Off
    UINT flags = sync_interval ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    HRESULT result = d3d12->swap_chain->Present(sync_interval, flags);

    if (result == DXGI_ERROR_DEVICE_REMOVED) {
        Assert(!"Failed to present.");
    }

    d3d12->frame_index = d3d12->swap_chain->GetCurrentBackBufferIndex();
}

static void d12Init(HWND hwnd) 
{
    IDXGIAdapter1 *adapter = nullptr;
    DXGI_ADAPTER_DESC1 adapter_desc = {};

    ID3D12Debug *debug_controller = nullptr;
    DWORD create_factory_flags = 0;

#if BUILD_DEBUG
    {
        // Enable Debug Layer
        if (SUCCEEDED( D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)) )) 
        {
            debug_controller->EnableDebugLayer();
            create_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        // GBV
        ID3D12Debug5 *debug_controller5 = nullptr;
        if (SUCCEEDED( debug_controller->QueryInterface(IID_PPV_ARGS(&debug_controller5)) )) 
        {
            debug_controller5->SetEnableGPUBasedValidation(TRUE);
            debug_controller5->SetEnableAutoName(TRUE);
            debug_controller5->Release();
        }
    }
#endif

    // Create DXGI-Factory
    //
    CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&d3d12->dxgi_factory));

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    int feature_level_count = CountOf(feature_levels);

    for (int i = 0; i < feature_level_count; ++i) 
    {
        int adapter_index = 0;
        while (d3d12->dxgi_factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) 
        {
            adapter->GetDesc1(&adapter_desc);

            if (SUCCEEDED(D3D12CreateDevice(adapter, feature_levels[i], IID_PPV_ARGS(&d3d12->Device)))) 
            {
                goto lb_exit;
            }

            adapter->Release();
            ++adapter_index;
        }
    }

lb_exit:
    Assert(d3d12->Device);
    d3d12->adapter_desc = adapter_desc;


#if BUILD_DEBUG
    if (debug_controller) 
    {
        ID3D12InfoQueue *info_queue = nullptr;
        d3d12->Device->QueryInterface(IID_PPV_ARGS(&info_queue));

        Assert(info_queue);

        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

		D3D12_MESSAGE_ID hide[] = {
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

        D3D12_INFO_QUEUE_FILTER filter = {};
        {
            filter.DenyList.NumIDs = (UINT)CountOf(hide);
            filter.DenyList.pIDList = hide;
        }

        info_queue->AddStorageFilterEntries(&filter);
        info_queue->Release();
    }
#endif

    // @Todo Do I need more?
    d3d12->CommandQueue = d12CreateCommandQueue();


    { // Create swap chain.
        d3d12->frame_count = 2;
        UINT res_w = 1920;
        UINT res_h = 1080;
        d12CreateSwapChain(hwnd, d3d12->CommandQueue, d3d12->frame_count, res_w, res_h);
    }


    { // Create RTV descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC Desc = {
            .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = d3d12->frame_count,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        };
        d3d12->Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&d3d12->rtv_heap));
        d3d12->rtv_descriptor_size = d3d12->Device->GetDescriptorHandleIncrementSize(Desc.Type);
    }

    
    { // Each frame gets RTV.
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(d3d12->rtv_heap->GetCPUDescriptorHandleForHeapStart());

        Assert(d3d12->frame_count <= MAX_FRAME_COUNT);
        for (UINT i = 0; i < d3d12->frame_count; ++i) {
            ASSERT_SUCCEEDED(d3d12->swap_chain->GetBuffer(i, IID_PPV_ARGS(d3d12->render_target_views + i)));
            d3d12->Device->CreateRenderTargetView(d3d12->render_target_views[i], nullptr, rtv_handle);
            rtv_handle.Offset(1, d3d12->rtv_descriptor_size);
        }
    }


    d3d12->fence = d12CreateFence();

    
    { // Set viewport, scissor rect.
        RECT Rect = {};
        GetClientRect(hwnd, &Rect);
        DWORD Width  = Rect.right  - Rect.left;
        DWORD Height = Rect.bottom - Rect.top;
        d3d12->Viewport = CD3DX12_VIEWPORT(0.f, 0.f, (FLOAT)Width, (FLOAT)Height);
        d3d12->ScissorRect = CD3DX12_RECT(0, 0, Width, Height);
    }

    ASSERT_SUCCEEDED( d3d12->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12->CommandAllocator)) );
    ASSERT_SUCCEEDED( d3d12->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->CommandAllocator, nullptr, IID_PPV_ARGS(&d3d12->CommandList)) );
    ASSERT_SUCCEEDED( d3d12->CommandList->Close() );

    // Create descriptor pool.
    //
    descriptor_pool *DescriptorPool = new descriptor_pool;
    memset(DescriptorPool, 0, sizeof(descriptor_pool));
    {
        DescriptorPool->MaxCount = 1024; // @Temporary
        DescriptorPool->DescriptorSize = d3d12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_DESCRIPTOR_HEAP_DESC CommonHeapDesc = {
            .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = DescriptorPool->MaxCount,
            .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        };

        ASSERT_SUCCEEDED( d3d12->Device->CreateDescriptorHeap(&CommonHeapDesc, IID_PPV_ARGS(&DescriptorPool->Heap)) );
        DescriptorPool->CPUHandle = DescriptorPool->Heap->GetCPUDescriptorHandleForHeapStart();
        DescriptorPool->GPUHandle = DescriptorPool->Heap->GetGPUDescriptorHandleForHeapStart();
    }

    // Create constant buffer pool.
    //
    constant_buffer_pool *ConstantBufferPool = new constant_buffer_pool;
    {
        const UINT CBVSize = (sizeof(constant_buffer) + 255) & ~255;

        ConstantBufferPool->MaxCount = 512; // @Temporary
        ConstantBufferPool->CBVSize = CBVSize;
        UINT Capacity = CBVSize*ConstantBufferPool->MaxCount;

        auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Capacity);
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ConstantBufferPool->Resource)) );


        // Create descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = ConstantBufferPool->MaxCount,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        };
        ASSERT_SUCCEEDED( d3d12->Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&ConstantBufferPool->CBVHeap)) );
        BYTE *CPUAddress = nullptr;
        CD3DX12_RANGE ReadRange(0, 0);
        ConstantBufferPool->Resource->Map(0, &ReadRange, (void **)&ConstantBufferPool->CPUAddress);


        D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc = {
            .BufferLocation = ConstantBufferPool->Resource->GetGPUVirtualAddress(),
            .SizeInBytes    = CBVSize
        };

        CD3DX12_CPU_DESCRIPTOR_HANDLE Handle(ConstantBufferPool->CBVHeap->GetCPUDescriptorHandleForHeapStart());

        UINT DescriptorSize = d3d12->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        ConstantBufferPool->CBVArray = new cbv_descriptor[ConstantBufferPool->MaxCount];
        for (UINT i = 0 ; i < ConstantBufferPool->MaxCount; ++i)
        {
            d3d12->Device->CreateConstantBufferView(&CBVDesc, Handle);

            ConstantBufferPool->CBVArray[i].Handle     = Handle;
            ConstantBufferPool->CBVArray[i].GPUAddress = CBVDesc.BufferLocation;
            ConstantBufferPool->CBVArray[i].CPUAddress = CPUAddress;

            Handle.Offset(1, DescriptorSize);
            CBVDesc.BufferLocation += CBVSize;
            CPUAddress += CBVSize;
        }
    }



    { // Cleanup
        if (debug_controller) { debug_controller->Release(); }
        if (adapter) { adapter->Release(); }
    }
}

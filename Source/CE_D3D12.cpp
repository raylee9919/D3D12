/* ========================================================================
   File: %
   Date: %
   Revision: %
   Creator: Seong Woo Lee
   Notice: (C) Copyright % by Seong Woo Lee. All Rights Reserved.
   ======================================================================== */

Internal D3D12_Fence d12CreateFence() {
    D3D12_Fence result = {};
    UINT64 initial_value = 0;
    d3d12->device->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&result.ptr));
    result.value = initial_value;
    result.event = CreateEventW(nullptr/*security attr*/, FALSE, FALSE, nullptr/*name*/);
    return result;
}

Internal void d12Fence(D3D12_Fence &fence) {
    fence.value += 1;
    d3d12->command_queue->Signal(fence.ptr, fence.value);
}

Internal void d12FenceWait(D3D12_Fence &fence) {
    UINT64 expected_value = fence.value;
    UINT64 current_value = fence.ptr->GetCompletedValue();

    // @Temporary
    if (current_value != expected_value) {
        fence.ptr->SetEventOnCompletion(expected_value, fence.event);
        WaitForSingleObject(fence.event, INFINITE);
    }
}

Internal ID3D12CommandQueue *d12CreateCommandQueue() {
    ID3D12CommandQueue *result = nullptr;
    D3D12_COMMAND_QUEUE_DESC desc = {}; 
    {
        desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    }

    ASSERT_SUCCEEDED(d3d12->device->CreateCommandQueue(&desc, IID_PPV_ARGS(&result)));
    return result;
}

Internal void d12CreateSwapChain(HWND hwnd, ID3D12CommandQueue *command_queue, UINT buffer_count, UINT width, UINT height) {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    {
        desc.Width       = width;
        desc.Height      = height;
        desc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Stereo      = 0;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = buffer_count;
        desc.Scaling     = DXGI_SCALING_NONE;
        desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
        desc.Flags       = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        desc.SampleDesc.Count = 1;
    }

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC full_screen_desc = {};
    {
        full_screen_desc.Windowed = TRUE;
    }

    IDXGISwapChain1 *swap_chain1 = nullptr;

    ASSERT_SUCCEEDED(d3d12->dxgi_factory->CreateSwapChainForHwnd(command_queue, hwnd, &desc, &full_screen_desc, nullptr, &swap_chain1));

    swap_chain1->QueryInterface(IID_PPV_ARGS(&d3d12->swap_chain));
    swap_chain1->Release();
    d3d12->frame_index = d3d12->swap_chain->GetCurrentBackBufferIndex();
}

Internal void d12Present() {
    UINT sync_interval = 1; // 1: On, 0: Off
    UINT flags = sync_interval ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    HRESULT result = d3d12->swap_chain->Present(sync_interval, flags);

    if (result == DXGI_ERROR_DEVICE_REMOVED) {
        Assert(!"Failed to present.");
    }

    d3d12->frame_index = d3d12->swap_chain->GetCurrentBackBufferIndex();
}

Internal void d12Init(HWND hwnd) {
    IDXGIAdapter1 *adapter = nullptr;
    DXGI_ADAPTER_DESC1 adapter_desc = {};

    ID3D12Debug *debug_controller = nullptr;
    DWORD create_factory_flags = 0;

#if BUILD_DEBUG
    {
        // Enable Debug Layer
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            debug_controller->EnableDebugLayer();
            create_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        // GBV
        ID3D12Debug5 *debug_controller5 = nullptr;
        if (SUCCEEDED(debug_controller->QueryInterface(IID_PPV_ARGS(&debug_controller5)))) {
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

    for (int i = 0; i < feature_level_count; ++i) {
        int adapter_index = 0;
        while (d3d12->dxgi_factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
            adapter->GetDesc1(&adapter_desc);

            if (SUCCEEDED(D3D12CreateDevice(adapter, feature_levels[i], IID_PPV_ARGS(&d3d12->device)))) {
                goto lb_exit;
            }

            adapter->Release();
            ++adapter_index;
        }
    }

lb_exit:
    Assert(d3d12->device);
    d3d12->adapter_desc = adapter_desc;


#if BUILD_DEBUG
    if (debug_controller) {
        ID3D12InfoQueue *info_queue = nullptr;
        d3d12->device->QueryInterface(IID_PPV_ARGS(&info_queue));

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
    d3d12->command_queue = d12CreateCommandQueue();


    { // Create swap chain.
        d3d12->frame_count = 2;
        UINT res_w = 1920;
        UINT res_h = 1080;
        d12CreateSwapChain(hwnd, d3d12->command_queue, d3d12->frame_count, res_w, res_h);
    }


    { // Create RTV descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        {
            desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = d3d12->frame_count;
            desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        }
        d3d12->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&d3d12->rtv_heap));
        d3d12->rtv_descriptor_size = d3d12->device->GetDescriptorHandleIncrementSize(desc.Type);
    }

    
    { // Each frame gets RTV.
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(d3d12->rtv_heap->GetCPUDescriptorHandleForHeapStart());

        Assert(d3d12->frame_count <= MAX_FRAME_COUNT);
        for (UINT i = 0; i < d3d12->frame_count; ++i) {
            ASSERT_SUCCEEDED(d3d12->swap_chain->GetBuffer(i, IID_PPV_ARGS(d3d12->render_target_views + i)));
            d3d12->device->CreateRenderTargetView(d3d12->render_target_views[i], nullptr, rtv_handle);
            rtv_handle.Offset(1, d3d12->rtv_descriptor_size);
        }
    }


    // @Note: Do I need more?
    ASSERT_SUCCEEDED(d3d12->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12->command_allocator)));

    // @Todo Do I need more?
    d3d12->fence = d12CreateFence();


    
    { // Set viewport, scissor rect.
        RECT rect = {};
        GetClientRect(hwnd, &rect);
        DWORD width  = rect.right  - rect.left;
        DWORD height = rect.bottom - rect.top;
        d3d12->viewport = CD3DX12_VIEWPORT(0.f, 0.f, (FLOAT)width, (FLOAT)height);
        d3d12->scissor_rect = CD3DX12_RECT(0, 0, width, height);
    }


    
    { // Create root signature.
        CD3DX12_ROOT_SIGNATURE_DESC desc = {};
        desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ID3DBlob *signature;
        ID3DBlob *error;
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        d3d12->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&d3d12->root_signature));
        if (signature) { signature->Release(); }
        if (error) { error->Release(); }
    }

    
    { // Create PSO.
        //D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        //{
        //    desc.pRootSignature         = d3d12->root_signature;
        //    desc.BlendState             = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        //    desc.SampleMask             = UINT_MAX;
        //    desc.RasterizerState        = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        //    desc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        //    desc.NumRenderTargets       = 1;
        //    desc.RTVFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;
        //    desc.SampleDesc.Count       = 1;
        //}
        //ASSERT_SUCCEEDED(d3d12->device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&d3d12->pipeline_state)));
    }


    // @Todo: Does it belong here?
    ASSERT_SUCCEEDED(d3d12->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->command_allocator, d3d12->pipeline_state, IID_PPV_ARGS(&d3d12->command_list)));
    ASSERT_SUCCEEDED(d3d12->command_list->Close());


    { // Cleanup
        if (debug_controller) { debug_controller->Release(); }
        if (adapter) { adapter->Release(); }
    }
}

// Copyright Seong Woo Lee. All Rights Reserved.

#include "D3D12_ConstantBufferPool.cpp"
#include "D3D12_DescriptorPool.cpp"

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
    if (current_value != expected_value) 
    {
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


    d3d12->m_Fence = d12CreateFence();

    
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

    ASSERT_SUCCEEDED( d3d12->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12->CommandAllocator)) );
    ASSERT_SUCCEEDED( d3d12->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->CommandAllocator, nullptr, IID_PPV_ARGS(&d3d12->CommandList)) );
    ASSERT_SUCCEEDED( d3d12->CommandList->Close() );

    
    d3d12->m_DescriptorPool = new descriptor_pool;
    d3d12->m_DescriptorPool->Init(1024, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    d3d12->m_SRVDescriptorPool = new descriptor_pool;
    d3d12->m_SRVDescriptorPool->Init(128, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    d3d12->m_ConstantBufferPool = new constant_buffer_pool;
    d3d12->m_ConstantBufferPool->Init(256, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);



    { // Cleanup
        if (debug_controller) { debug_controller->Release(); }
        if (adapter) { adapter->Release(); }
    }
}

void D3D12_State::End()
{
    m_DescriptorPool->Clear();
    m_ConstantBufferPool->Clear();
}

static texture *CreateTexture(void *Data, UINT Width, UINT Height, DXGI_FORMAT Format)
{
    texture* Result = new texture;
    ID3D12Resource *Resource = nullptr;

    {
        D3D12_RESOURCE_DESC TextureDesc = {
            .Dimension =  D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = Width,
            .Height = Height,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = Format,
            .SampleDesc = {
                .Count = 1,
                .Quality = 0,
            },
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };

        auto DefaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&DefaultHeapProp, D3D12_HEAP_FLAG_NONE, &TextureDesc, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&Resource) ) );

        if (Data)
        {
            D3D12_RESOURCE_DESC Desc = Resource->GetDesc();
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
            {
                d3d12->Device->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, nullptr, nullptr, nullptr);
            }

            BYTE *Mapped = nullptr;
            CD3DX12_RANGE ReadRange(0, 0);

            UINT64 UploadBufferSize = GetRequiredIntermediateSize(Resource, 0, 1);

            ID3D12Resource *UploadBuffer = nullptr;

            auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);
            ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)) );

            ASSERT_SUCCEEDED( UploadBuffer->Map(0, &ReadRange, (void **)&Mapped) );

            BYTE *Src = (BYTE *)Data;
            BYTE *Dst = Mapped;
            UINT Stride = Width*4; // @Hack
            for (UINT Y = 0; Y < Height; Y++)
            {
                memcpy(Dst, Src, Stride);
                Src += Stride;
                Dst += Footprint.Footprint.RowPitch;
            }
            UploadBuffer->Unmap(0, nullptr);

            {
                const DWORD MAX_SUB_RESOURCE_NUM = 32;
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint[MAX_SUB_RESOURCE_NUM] = {};
                UINT	Rows[MAX_SUB_RESOURCE_NUM] = {};
                UINT64	RowSize[MAX_SUB_RESOURCE_NUM] = {};
                UINT64	TotalBytes = 0;

                D3D12_RESOURCE_DESC Desc = Resource->GetDesc();
                Assert( Desc.MipLevels <= (UINT)_countof(Footprint) );

                d3d12->Device->GetCopyableFootprints(&Desc, 0, Desc.MipLevels, 0, Footprint, Rows, RowSize, &TotalBytes);

                ASSERT_SUCCEEDED( d3d12->CommandAllocator->Reset() );

                ASSERT_SUCCEEDED( d3d12->CommandList->Reset(d3d12->CommandAllocator, nullptr) );

                auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(Resource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
                d3d12->CommandList->ResourceBarrier(1, &Transition1);
                for (DWORD i = 0; i < Desc.MipLevels; i++)
                {
                    D3D12_TEXTURE_COPY_LOCATION	destLocation = {};
                    destLocation.PlacedFootprint = Footprint[i];
                    destLocation.pResource = Resource;
                    destLocation.SubresourceIndex = i;
                    destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

                    D3D12_TEXTURE_COPY_LOCATION	srcLocation = {};
                    srcLocation.PlacedFootprint = Footprint[i];
                    srcLocation.pResource = UploadBuffer;
                    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

                    d3d12->CommandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
                }
                auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(Resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                d3d12->CommandList->ResourceBarrier(1, &Transition2);
                d3d12->CommandList->Close();

                ID3D12CommandList *CommandLists[] = { d3d12->CommandList };
                d3d12->CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);


                d12Fence(d3d12->m_Fence);
                d12FenceWait(d3d12->m_Fence);
            }

            UploadBuffer->Release();
        }
    }


    CD3DX12_CPU_DESCRIPTOR_HANDLE SRVHandle = {};
    d3d12->m_SRVDescriptorPool->Alloc(&SRVHandle, nullptr, 1);
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
        .Format = Format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {
            .MipLevels = 1,
        },
    };
    d3d12->Device->CreateShaderResourceView(Resource, &SRVDesc, SRVHandle);

    Result->m_Resource  = Resource;
    Result->m_CPUHandle = SRVHandle;
    return Result;
}

static texture *CreateTextureFromFile(const WCHAR *FileName)
{
    ID3D12Resource *TexResource  = nullptr;
    ID3D12Resource *UploadBuffer = nullptr;

    std::unique_ptr<uint8_t[]> Data;
    std::vector<D3D12_SUBRESOURCE_DATA> SubresourceData;

    ASSERT_SUCCEEDED( DirectX::LoadDDSTextureFromFile(d3d12->Device, FileName, &TexResource, Data, SubresourceData) );

    D3D12_RESOURCE_DESC TexDesc = TexResource->GetDesc();
    UINT SubresourceSize = (UINT)SubresourceData.size();
    UINT64 UploadBufferSize = GetRequiredIntermediateSize(TexResource, 0, SubresourceSize);

    auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);
    ASSERT_SUCCEEDED( d3d12->Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)) );

    ID3D12CommandAllocator *CmdAllocator = d3d12->CommandAllocator;
    ID3D12GraphicsCommandList *CmdList = d3d12->CommandList;
    ID3D12CommandQueue *CmdQueue = d3d12->CommandQueue;

    ASSERT_SUCCEEDED( CmdAllocator->Reset() );
    ASSERT_SUCCEEDED( CmdList->Reset(CmdAllocator, nullptr) );

    auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(TexResource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    CmdList->ResourceBarrier(1, &Transition1);
    UpdateSubresources(CmdList, TexResource, UploadBuffer, 0, 0, SubresourceSize, &SubresourceData[0]);
    auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(TexResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    CmdList->ResourceBarrier(1, &Transition2);
    CmdList->Close();

    ID3D12CommandList *CmdLists[] = { CmdList };
    CmdQueue->ExecuteCommandLists(_countof(CmdLists), CmdLists);

    d12Fence(d3d12->m_Fence);
    d12FenceWait(d3d12->m_Fence);

    if (UploadBuffer)
    {
        UploadBuffer->Release();
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE SRVHandle = {};
    d3d12->m_SRVDescriptorPool->Alloc(&SRVHandle, nullptr, 1);
    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
        .Format = TexDesc.Format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {
            .MipLevels = TexDesc.MipLevels,
        }
    };
    d3d12->Device->CreateShaderResourceView(TexResource, &SRVDesc, SRVHandle);

    texture *Result = new texture;
    Result->m_Resource = TexResource;
    Result->m_CPUHandle = SRVHandle;

    return Result;
}

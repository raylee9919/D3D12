// Copyright Seong Woo Lee. All Rights Reserved.

#include "D3D12_ResourceManager.cpp"
#include "D3D12_CommandListPool.cpp"
#include "D3D12_ConstantBufferPool.cpp"
#include "D3D12_DescriptorPool.cpp"
#include "D3D12_MeshObject.cpp"
#include "D3D12_RenderQueue.cpp"


void InitRenderer(d3d12_renderer* Renderer, HWND Window, UINT Width, UINT Height)
{
    Renderer->Init(Window, Width, Height);
}

void d3d12_renderer::Init(HWND hWindow, UINT FrameBufferWidth, UINT FrameBufferHeight)
{
    IDXGIAdapter1 *Adapter = nullptr;
    DXGI_ADAPTER_DESC1 AdapterDesc = {};

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
            Adapter->GetDesc1(&AdapterDesc);

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
    d3d12->m_AdapterDesc = AdapterDesc;


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
            d3d12->m_CommandListPools[i][j] = new command_list_pool;
            d3d12->m_CommandListPools[i][j]->Init(d3d12->Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        }
    }


    { // Create swap chain.
        // @Temporary
        CreateSwapChain(hWindow, SWAPCHAIN_FRAME_COUNT, FrameBufferWidth, FrameBufferHeight, m_CommandQueue);
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
    GetClientRect(hWindow, &Rect);
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


    CreateDepthStencil(Width, Height, m_DSVHeap, &m_DepthStencilResource);


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
        Context->Renderer = this;
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

i_mesh* d3d12_renderer::CreateMesh(void* Vertices, u32 VertexSize, u32 VerticesCount, void* Indices, u32 IndexSize, u32 IndicesCount)
{
    d3d12_mesh* Result = new d3d12_mesh;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
    ID3D12Resource* VertexBuffer = nullptr;

    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
    ID3D12Resource* IndexBuffer = nullptr;

    m_ResourceManager->CreateVertexBuffer(VertexSize, VerticesCount, Vertices, &VertexBufferView, &VertexBuffer);
    m_ResourceManager->CreateIndexBuffer(IndexSize, IndicesCount, Indices, &IndexBufferView, &IndexBuffer);


    { // Create root signature and pipeline state.
        ID3D12RootSignature* RootSignature = nullptr;
        ID3D12PipelineState* PipelineState = nullptr;

        ID3DBlob* Signature = nullptr;
        ID3DBlob* Error = nullptr;

        CD3DX12_DESCRIPTOR_RANGE RangesPerObj[1] = {};
        RangesPerObj[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0

        CD3DX12_DESCRIPTOR_RANGE RangesPerGroup[1] = {};
        RangesPerGroup[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

        CD3DX12_ROOT_PARAMETER RootParamters[2] = {};
        RootParamters[0].InitAsDescriptorTable(_countof(RangesPerObj), RangesPerObj, D3D12_SHADER_VISIBILITY_ALL);
        RootParamters[1].InitAsDescriptorTable(_countof(RangesPerGroup), RangesPerGroup, D3D12_SHADER_VISIBILITY_ALL);

        UINT SamplerRegisterIndex = 0;
        D3D12_STATIC_SAMPLER_DESC SamplerDesc = {
            .Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            .MipLODBias       = 0.f,
            .MaxAnisotropy    = 16,
            .ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER,
            .BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
            .MinLOD           = -FLT_MAX,
            .MaxLOD           = D3D12_FLOAT32_MAX,
            .ShaderRegister   = SamplerRegisterIndex,
            .RegisterSpace    = 0,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
        };
        SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

        CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
        RootSignatureDesc.Init(_countof(RootParamters), RootParamters, 1, &SamplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


        ASSERT_SUCCEEDED( D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error) );
        ASSERT_SUCCEEDED( Device->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature)) );

        if (Signature)
        {
            Signature->Release();
        }
        if (Error)
        {
            Error->Release();
        }

        // Create pipeline state.
        //
        ID3DBlob* VertexShader = nullptr;
        ID3DBlob* PixelShader  = nullptr;
        ID3DBlob* ErrorBlob    = nullptr;

        UINT CompilerFlags = 0;
#if BUILD_DEBUG
        CompilerFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        // @Temporary
        if (FAILED( D3DCompileFromFile(L"./Assets/Shaders/Simple.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", CompilerFlags, 0, &VertexShader, &ErrorBlob) ))
        {
            LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
            OutputDebugStringA(Error);
            ASSERT( !"Vertex Shader Compilation Failed." );
        }
        if (FAILED( D3DCompileFromFile(L"./Assets/Shaders/Simple.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", CompilerFlags, 0, &PixelShader, &ErrorBlob) ))
        {
            LPCSTR Error = (LPCSTR)ErrorBlob->GetBufferPointer();
            OutputDebugStringA(Error);
            ASSERT( !"Pixel Shader Compilation Failed." );
        }

        // @Temporary
        D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(vertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertex, TexCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {
            .pRootSignature        = RootSignature,
            .VS                    = CD3DX12_SHADER_BYTECODE(VertexShader->GetBufferPointer(), VertexShader->GetBufferSize()),
            .PS                    = CD3DX12_SHADER_BYTECODE(PixelShader->GetBufferPointer(), PixelShader->GetBufferSize()),
            .BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask            = UINT_MAX,
            .RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .InputLayout           = { InputElementDescs, _countof(InputElementDescs) }, 
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets      = 1,
            .DSVFormat             = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {
                .Count = 1,
            }
        };
        PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        PsoDesc.RasterizerState.FrontCounterClockwise = true;
        PsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        PsoDesc.DepthStencilState.StencilEnable = FALSE;

        ASSERT_SUCCEEDED( Device->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PipelineState)) );

        if (VertexShader)
        {
            VertexShader->Release();
        }

        if (PixelShader)
        {
            PixelShader->Release();
        }

        Result->m_RootSignature = RootSignature;
        Result->m_PipelineState = PipelineState;
    }

    submesh *Submesh = new submesh;
    {
        Submesh->m_IndexCount      = IndicesCount;
        Submesh->m_IndexBuffer     = IndexBuffer;
        Submesh->m_IndexBufferView = IndexBufferView;
        Submesh->m_Texture         = m_ResourceManager->CreateTextureFromFileName(L"Assets/AnimeGirl0.dds", (UINT)wcslen(L"Assets/AnimeGirl0.dds"));
    }

    // @Temporary
    Result->m_Submesh.push_back(Submesh);
    Result->m_SubmeshCount++;

    Result->m_VertexBuffer = VertexBuffer;
    Result->m_VertexBufferView = VertexBufferView;
    Result->m_IndexBuffer = IndexBuffer;
    Result->m_IndexBufferView = IndexBufferView;

    return Result;
}

void d3d12_renderer::CreateSwapChain(HWND hWindow, UINT BufferCount, UINT Width, UINT Height, ID3D12CommandQueue* CommandQueue) 
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
    m_SwapChainFlags = Desc.Flags;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC FullScreenDesc = {
        .Windowed = TRUE
    };

    IDXGISwapChain1* SwapChain1 = nullptr;

    ASSERT_SUCCEEDED( dxgi_factory->CreateSwapChainForHwnd(CommandQueue, hWindow, &Desc, &FullScreenDesc, nullptr, &SwapChain1) );

    SwapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
    SwapChain1->Release();
    m_RenderTargetIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void d3d12_renderer::CreateDepthStencil(UINT Width, UINT Height, ID3D12DescriptorHeap* DescriptorHeap, ID3D12Resource** OutDsvResource)
{
    ASSERT(DescriptorHeap);

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
        ASSERT_SUCCEEDED( Device->CreateCommittedResource(&DefaultHeapProp, D3D12_HEAP_FLAG_NONE, &DepthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &ClearValue, IID_PPV_ARGS(OutDsvResource)) );
        (*OutDsvResource)->SetName(L"m_DepthStencilResource");

        CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle(DescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        Device->CreateDepthStencilView(*OutDsvResource, &DSVDesc, DSVHandle);
    }
}

void d3d12_renderer::UpdateFramebuffer(UINT Width, UINT Height)
{
    ASSERT( Width!=0 || Height!=0 );

    for (UINT i = 0; i < MAX_PENDING_FRAME_COUNT; ++i)
    {
        FenceWait(m_LastFenceValues[i]);
    }

    // Swapchain
    DXGI_SWAP_CHAIN_DESC1 Desc = {};
    ASSERT_SUCCEEDED( m_SwapChain->GetDesc1(&Desc) );

    for (int i = 0; i < SWAPCHAIN_FRAME_COUNT; ++i)
    {
        m_RenderTargets[i]->Release();
        m_RenderTargets[i] = nullptr;
    }

    ASSERT_SUCCEEDED( m_SwapChain->ResizeBuffers(SWAPCHAIN_FRAME_COUNT, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, m_SwapChainFlags) );

    m_RenderTargetIndex = m_SwapChain->GetCurrentBackBufferIndex();

    CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle = GetRtvHandle();

    for (int i = 0; i < SWAPCHAIN_FRAME_COUNT; ++i)
    {
        m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i]));
        Device->CreateRenderTargetView(m_RenderTargets[i], nullptr, RTVHandle);
        RTVHandle.Offset(1, m_RTVDescriptorSize);
    }


    // Depth, Stencil
    if (m_DepthStencilResource)
    {
        m_DepthStencilResource->Release();
        m_DepthStencilResource = nullptr;
    }
    CreateDepthStencil(Width, Height, m_DSVHeap, &m_DepthStencilResource);


    // Viewport and scissor rect.
    m_Viewport.Width  = (FLOAT)Width;
    m_Viewport.Height = (FLOAT)Height;
    m_ScissorRect.left   = 0;
    m_ScissorRect.top    = 0;
    m_ScissorRect.right  = Width;
    m_ScissorRect.bottom = Height;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE d3d12_renderer::GetRtvHandle()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), d3d12->m_RenderTargetIndex, d3d12->m_RTVDescriptorSize);
    return RtvHandle;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE d3d12_renderer::GetDsvHandle()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE DsvHandle(m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
    return DsvHandle;
}

DWORD WINAPI RenderThreadProc(LPVOID Param)
{
    render_thread_context* Context = (render_thread_context*)Param;
    d3d12_renderer* Renderer = Context->Renderer;
    const UINT ThreadIndex = Context->Index;
    for(;;)
    {
        WaitForSingleObject(Context->Event, INFINITE);

        command_list_pool* CommandListPool = Renderer->m_CommandListPools[Renderer->m_CurrentContextIndex][ThreadIndex];
        descriptor_pool* DescriptorPool = Renderer->m_DescriptorPools[Renderer->m_CurrentContextIndex][ThreadIndex];
        constant_buffer_pool* ConstantBufferPool = Renderer->m_ConstantBufferPools[Renderer->m_CurrentContextIndex][ThreadIndex];

        CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle = Renderer->GetRtvHandle();
        CD3DX12_CPU_DESCRIPTOR_HANDLE DsvHandle = Renderer->GetDsvHandle();

        Renderer->m_RenderQueues[ThreadIndex]->Process(RtvHandle, DsvHandle, CommandListPool, Renderer->m_CommandQueue, DescriptorPool, ConstantBufferPool, 400);

        LONG RemainThreadCount = InterlockedDecrement(&Renderer->m_RemainThreadCount);
        if (RemainThreadCount == 0) 
        {
            SetEvent(Renderer->m_CompletionEvent);
        }
    }

    //return 0;
}

UINT64 d3d12_renderer::Fence()
{
    m_FenceValue++;
    m_CommandQueue->Signal(m_Fence, m_FenceValue);
    m_LastFenceValues[m_CurrentContextIndex] = m_FenceValue;
    return m_FenceValue;
}

void d3d12_renderer::FenceWait(UINT64 ExpectedValue) 
{
    if (m_Fence->GetCompletedValue() < ExpectedValue) 
    {
        m_Fence->SetEventOnCompletion(ExpectedValue, m_FenceEvent);
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }
}

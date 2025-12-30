// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

void D3D12::Begin()
{
    // Clear
    w32Assert(m_CommandAllocator->Reset());
    w32Assert(m_CommandList->Reset(m_CommandAllocator, m_PipelineState));

    // Set necessary state.
    m_CommandList->SetGraphicsRootSignature(m_RootSignature);
    m_CommandList->SetDescriptorHeaps(1, &m_DescriptorHeap);
    m_CommandList->SetGraphicsRootDescriptorTable(0, m_DescriptorGPUHandle);

    m_CommandList->RSSetViewports(1, &m_Viewport);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

    auto ResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_RenderTargetIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &ResourceBarrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_RenderTargetIndex, m_RTVDescriptorSize);
    m_CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, nullptr);

    // Record commands.
    const float BackColor[] = { 0.5f, 0.2f, 0.2f, 1.0f };
    m_CommandList->ClearRenderTargetView(RTVHandle, BackColor, 0, nullptr);
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    m_CommandList->IASetIndexBuffer(&m_IndexBufferView);
    m_CommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void D3D12::End()
{
    auto ResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_RenderTargetIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &ResourceBarrier);
    m_CommandList->Close();

    ID3D12CommandList *CommandLists[] = { m_CommandList };
    m_CommandQueue->ExecuteCommandLists(CountOf(CommandLists), CommandLists);
}

void D3D12::LoadAssets()
{
    InitRootSignature();

    { // Create the pipeline state, which includes compiling and loading shaders.
        ID3DBlob *VertexShader = nullptr;
        ID3DBlob *PixelShader  = nullptr;
        ID3DBlob *Error        = nullptr;

#if _DEBUG
        UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT CompileFlags = 0;
#endif

        if (FAILED(D3DCompileFromFile(L"../Assets/Shaders/Shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", CompileFlags, 0, &VertexShader, &Error))) {
            const char *Msg = (const char *)Error->GetBufferPointer();
            OutputDebugStringA(Msg);
            Assert(0);
        }
        if (FAILED((D3DCompileFromFile(L"../Assets/Shaders/Shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", CompileFlags, 0, &PixelShader, &Error)))) {
            const char *Msg = (const char *)Error->GetBufferPointer();
            OutputDebugStringA(Msg);
            Assert(0);
        }


        D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc = {};
        {
            PSODesc.pRootSignature = m_RootSignature;
            PSODesc.VS = CD3DX12_SHADER_BYTECODE(VertexShader);
            PSODesc.PS = CD3DX12_SHADER_BYTECODE(PixelShader);
            PSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            PSODesc.SampleMask = UINT_MAX;
            PSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            PSODesc.DepthStencilState.DepthEnable = FALSE;
            PSODesc.DepthStencilState.StencilEnable = FALSE;
            PSODesc.InputLayout = { InputElementDescs, CountOf(InputElementDescs) };
            PSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            PSODesc.NumRenderTargets = 1;
            PSODesc.SampleDesc.Count = 1;
        }
        PSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        w32Assert(m_Device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&m_PipelineState)));

        if (VertexShader) { VertexShader->Release(); }
        if (PixelShader) { PixelShader->Release(); }
        if (Error) { Error->Release(); }
    }

    { // Create Command List.
        w32Assert(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator, m_PipelineState, IID_PPV_ARGS(&m_CommandList)));
        m_CommandList->Close();
    }
}

void D3D12::InitRootSignature()
{
    ID3DBlob *Signature = nullptr;
    ID3DBlob *Error = nullptr;

    CD3DX12_DESCRIPTOR_RANGE Ranges[2] = {};
    Ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    Ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER RootParameters[1] = {};
    RootParameters[0].InitAsDescriptorTable(CountOf(Ranges), Ranges, D3D12_SHADER_VISIBILITY_ALL);

    UINT RegisterIndex = 0;
    D3D12_STATIC_SAMPLER_DESC SamplerDesc = {};
    {
        SamplerDesc.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
        SamplerDesc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        SamplerDesc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        SamplerDesc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        SamplerDesc.MipLODBias       = 0.0f;
        SamplerDesc.MaxAnisotropy    = 16;
        SamplerDesc.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
        SamplerDesc.BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        SamplerDesc.MinLOD           = -FLT_MAX;
        SamplerDesc.MaxLOD           = D3D12_FLOAT32_MAX;
        SamplerDesc.ShaderRegister   = RegisterIndex;
        SamplerDesc.RegisterSpace    = 0;
        SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
    RootSignatureDesc.Init(CountOf(RootParameters), RootParameters, 1, &SamplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    w32Assert(D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, &Error));
    w32Assert(m_Device->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));

    if (Signature) { Signature->Release(); }
    if (Error) { Error->Release(); }
}

void D3D12::CreateVertexBuffer(UINT32 VertexCount, Vertex *Vertices)
{
    ID3D12Resource *UploadBuffer = nullptr;
    ID3D12Resource *VertexBuffer = nullptr;

    UINT VertexSize = sizeof(Vertices[0]);
    UINT VertexBufferSize = VertexCount*VertexSize;

    auto HeapPropDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto HeapPropUpload  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto Desc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);

    w32Assert(m_Device->CreateCommittedResource(&HeapPropDefault, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&VertexBuffer)));
    w32Assert(m_Device->CreateCommittedResource(&HeapPropUpload,  D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&UploadBuffer)));

    UINT8 *UploadBase = nullptr;
    CD3DX12_RANGE ReadRange(0, 0);
    Assert(SUCCEEDED(UploadBuffer->Map(0, &ReadRange, reinterpret_cast<void **>(&UploadBase))));
    memcpy(UploadBase, Vertices, VertexBufferSize);
    UploadBuffer->Unmap(0, nullptr);

    auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(VertexBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(VertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    w32Assert(m_CommandAllocator->Reset());
    w32Assert(m_CommandList->Reset(m_CommandAllocator, nullptr));
    m_CommandList->ResourceBarrier(1, &Transition1);
    m_CommandList->CopyBufferRegion(VertexBuffer, 0, UploadBuffer, 0, VertexBufferSize);
    m_CommandList->ResourceBarrier(1, &Transition2);
    m_CommandList->Close();

    ID3D12CommandList *CommandLists[] = { m_CommandList };
   m_CommandQueue->ExecuteCommandLists(CountOf(CommandLists), CommandLists);

    Fence();
    WaitForFenceValue();

    m_VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.StrideInBytes  = VertexSize;
    m_VertexBufferView.SizeInBytes    = VertexBufferSize;

    m_VertexBuffer = VertexBuffer;
}

void D3D12::CreateIndexBuffer(UINT32 IndexCount, UINT16 *Indices)
{
    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
    ID3D12Resource *IndexBuffer  = nullptr;
    ID3D12Resource *UploadBuffer = nullptr;
    UINT IndexBufferSize = IndexCount*sizeof(UINT16);

    auto HeapPropDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto HeapPropUpload  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto Desc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferSize);

    w32Assert(m_Device->CreateCommittedResource(&HeapPropDefault, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&IndexBuffer)));
    w32Assert(m_Device->CreateCommittedResource(&HeapPropUpload,  D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&UploadBuffer)));

    UINT8 *UploadBase = nullptr;
    CD3DX12_RANGE ReadRange(0, 0);
    Assert(SUCCEEDED(UploadBuffer->Map(0, &ReadRange, reinterpret_cast<void **>(&UploadBase))));
    memcpy(UploadBase, Indices, IndexBufferSize);
    UploadBuffer->Unmap(0, nullptr);

    auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(IndexBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(IndexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    w32Assert(m_CommandAllocator->Reset());
    w32Assert(m_CommandList->Reset(m_CommandAllocator, nullptr));
    m_CommandList->ResourceBarrier(1, &Transition1);
    m_CommandList->CopyBufferRegion(IndexBuffer, 0, UploadBuffer, 0, IndexBufferSize);
    m_CommandList->ResourceBarrier(1, &Transition2);
    m_CommandList->Close();

    ID3D12CommandList *CommandLists[] = { m_CommandList };
    m_CommandQueue->ExecuteCommandLists(CountOf(CommandLists), CommandLists);

    Fence();
    WaitForFenceValue();

    m_IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format         = DXGI_FORMAT_R16_UINT;
    m_IndexBufferView.SizeInBytes    = IndexBufferSize;

    m_IndexBuffer = IndexBuffer;
}

void D3D12::CreateTexture(UINT32 Width, UINT32 Height, DXGI_FORMAT Format, UINT32 *Data)
{
    ID3D12Resource *UploadBuffer = nullptr;

    D3D12_RESOURCE_DESC Desc = {}; 
    {
        Desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        Desc.Width              = Width;
        Desc.Height             = Height;
        Desc.DepthOrArraySize   = 1;
        Desc.MipLevels          = 1;
        Desc.Format             = Format;
        Desc.SampleDesc.Count   = 1;
        Desc.SampleDesc.Quality = 0;
        Desc.Flags              = D3D12_RESOURCE_FLAG_NONE;
    }

    auto HeapPropDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto HeapPropUpload  = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    w32Assert(m_Device->CreateCommittedResource(&HeapPropDefault, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_Texture)));

    UINT64 Size = GetRequiredIntermediateSize(m_Texture, 0, 1);
    auto UploadDesc = CD3DX12_RESOURCE_DESC::Buffer(Size);
    w32Assert(m_Device->CreateCommittedResource(&HeapPropUpload, D3D12_HEAP_FLAG_NONE, &UploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)));

    D3D12_SUBRESOURCE_DATA TexData = {};
    {
        TexData.pData      = Data;
        TexData.RowPitch   = Width*4;
        TexData.SlicePitch = TexData.RowPitch*Height;
    }

    w32Assert(m_CommandAllocator->Reset());
    w32Assert(m_CommandList->Reset(m_CommandAllocator, nullptr));

    UpdateSubresources(m_CommandList, m_Texture, UploadBuffer, 0, 0, 1, &TexData);

    auto TransitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_Texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_CommandList->ResourceBarrier(1, &TransitionBarrier);

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    {
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Format                  = Format;
        SRVDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MipLevels     = 1;
    }
    CD3DX12_CPU_DESCRIPTOR_HANDLE SRV(m_DescriptorCPUHandle, DESCRIPTOR_INDEX_TEX, m_DescriptorSize);
    m_Device->CreateShaderResourceView(m_Texture, &SRVDesc, SRV);

    w32Assert(m_CommandList->Close());
    ID3D12CommandList *CommandLists[] = { m_CommandList };
    m_CommandQueue->ExecuteCommandLists(CountOf(CommandLists), CommandLists);

    Fence();
    WaitForFenceValue();
}

void D3D12::CreateConstantBuffer()
{
    // @Temporary: We are mapping the constant buffer and not unmapping for the entirety of the application.
    //

    // Constant buffer is aligned to be 256-bytes.
    UINT Size = ((sizeof(Constant_Buffer) + 255) & ~255);

    auto PropUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);
    w32Assert(m_Device->CreateCommittedResource(&PropUpload, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_ConstantBuffer)));

    D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc = {};
    {
        CBVDesc.BufferLocation = m_ConstantBuffer->GetGPUVirtualAddress();
        CBVDesc.SizeInBytes    = Size;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE CBV(m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), DESCRIPTOR_INDEX_CBV, m_DescriptorSize);
    m_Device->CreateConstantBufferView(&CBVDesc, CBV);

    CD3DX12_RANGE ReadRange(0, 0);
    w32Assert(m_ConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&m_SysMappedConstantBuffer)));

    m_SysMappedConstantBuffer->Offset.X = 0.0f;
    m_SysMappedConstantBuffer->Offset.Y = 0.0f;
}

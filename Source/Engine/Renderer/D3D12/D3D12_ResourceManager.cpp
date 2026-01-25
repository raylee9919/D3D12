// Copyright Seong Woo Lee. All Rights Reserved.

void resource_manager::Init(ID3D12Device5* Device)
{
    m_Device = Device;

    m_SRVDescriptorPool = new descriptor_pool;
    m_SRVDescriptorPool->Init(2048, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    D3D12_COMMAND_QUEUE_DESC QueueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
    };

    // Create comamnd queue.
    ASSERT_SUCCEEDED( Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&m_CommandQueue)) );

    // Create comamnd allocator.
    ASSERT_SUCCEEDED( Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator)) );

    // Create comamnd list.
    ASSERT_SUCCEEDED( Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator, nullptr, IID_PPV_ARGS(&m_CommandList)) );
    m_CommandList->Close();

    // Create fence.
    ASSERT_SUCCEEDED( Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)) );
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_FenceValue = 0;
}

void resource_manager::CreateVertexBuffer(UINT VertexSize, UINT VerticesCount, void* Vertices,
                                          D3D12_VERTEX_BUFFER_VIEW* OutVertexBufferView, ID3D12Resource** OutBuffer)
{
    const UINT Size = VertexSize*VerticesCount;

    ID3D12Resource* VertexBuffer = nullptr;
    ID3D12Resource* UploadBuffer = nullptr;


    auto DefaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Size);
    ASSERT_SUCCEEDED( m_Device->CreateCommittedResource(&DefaultHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&VertexBuffer)) );

    ASSERT_SUCCEEDED( m_CommandAllocator->Reset() );
    ASSERT_SUCCEEDED( m_CommandList->Reset(m_CommandAllocator, nullptr) );

    ASSERT_SUCCEEDED( m_Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&UploadBuffer)) );

    void* Mapped = nullptr;
    CD3DX12_RANGE ReadRange(0, 0);
    ASSERT_SUCCEEDED( UploadBuffer->Map(0, &ReadRange, &Mapped) );
    memcpy(Mapped, Vertices, Size);
    UploadBuffer->Unmap(0, nullptr);

    const auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(VertexBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    const auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(VertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    m_CommandList->ResourceBarrier(1, &Transition1);
    m_CommandList->CopyBufferRegion(VertexBuffer, 0, UploadBuffer, 0, Size);
    m_CommandList->ResourceBarrier(1, &Transition2);
    m_CommandList->Close();

    ID3D12CommandList* CommandLists[] = { m_CommandList };
    m_CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

    Fence();
    FenceWait();


    OutVertexBufferView->BufferLocation = VertexBuffer->GetGPUVirtualAddress();
    OutVertexBufferView->SizeInBytes    = Size;
    OutVertexBufferView->StrideInBytes  = VertexSize;
    *OutBuffer = VertexBuffer;

    if (UploadBuffer)
    {
        UploadBuffer->Release();
    }
}

void resource_manager::CreateIndexBuffer(UINT IndexSize, UINT IndicesCount, void* Indices, D3D12_INDEX_BUFFER_VIEW* OutIndexBufferView, ID3D12Resource** OutBuffer)
{
    ASSERT(IndexSize == 2); // UINT16

    const UINT Size = IndexSize*IndicesCount;

    ID3D12Resource* IndexBuffer = nullptr;
    ID3D12Resource* UploadBuffer = nullptr;

    auto DefaultHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Size);
    ASSERT_SUCCEEDED( m_Device->CreateCommittedResource(&DefaultHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&IndexBuffer)) );

    ASSERT_SUCCEEDED( m_CommandAllocator->Reset() );
    ASSERT_SUCCEEDED( m_CommandList->Reset(m_CommandAllocator, nullptr) );

    ASSERT_SUCCEEDED( m_Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&UploadBuffer)) );

    void* Mapped = nullptr;
    CD3DX12_RANGE ReadRange(0, 0);
    ASSERT_SUCCEEDED( UploadBuffer->Map(0, &ReadRange, &Mapped) );
    memcpy(Mapped, Indices, Size);
    UploadBuffer->Unmap(0, nullptr);

    const auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(IndexBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    const auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(IndexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    m_CommandList->ResourceBarrier(1, &Transition1);
    m_CommandList->CopyBufferRegion(IndexBuffer, 0, UploadBuffer, 0, Size);
    m_CommandList->ResourceBarrier(1, &Transition2);
    m_CommandList->Close();

    ID3D12CommandList* CommandLists[] = { m_CommandList };
    m_CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists );

    Fence();
    FenceWait();

    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {
        .BufferLocation = IndexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes    = Size,
        .Format         = DXGI_FORMAT_R16_UINT, // @Robustness
    };

    *OutIndexBufferView = IndexBufferView;
    *OutBuffer = IndexBuffer;

    if (UploadBuffer)
    {
        UploadBuffer->Release();
    }
}

texture* resource_manager::CreateTextureFromFileName(const WCHAR* FileName, UINT Length)
{
    texture* Result = nullptr;
    std::wstring FileNameString(FileName, Length);

    if (m_TextureTable.find(FileNameString) == m_TextureTable.end())
    {
        ID3D12Resource* TexResource  = nullptr;
        ID3D12Resource* UploadBuffer = nullptr;

        std::unique_ptr<uint8_t[]> Data;
        std::vector<D3D12_SUBRESOURCE_DATA> SubresourceData;

        ASSERT_SUCCEEDED( DirectX::LoadDDSTextureFromFile(m_Device, FileName, &TexResource, Data, SubresourceData) );

        D3D12_RESOURCE_DESC TexDesc = TexResource->GetDesc();
        UINT SubresourceSize = (UINT)SubresourceData.size();
        UINT64 UploadBufferSize = GetRequiredIntermediateSize(TexResource, 0, SubresourceSize);

        auto UploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);
        ASSERT_SUCCEEDED( m_Device->CreateCommittedResource(&UploadHeapProp, D3D12_HEAP_FLAG_NONE, &BufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&UploadBuffer)) );

        ASSERT_SUCCEEDED( m_CommandAllocator->Reset() );
        ASSERT_SUCCEEDED( m_CommandList->Reset(m_CommandAllocator, nullptr) );

        auto Transition1 = CD3DX12_RESOURCE_BARRIER::Transition(TexResource, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        m_CommandList->ResourceBarrier(1, &Transition1);
        UpdateSubresources(m_CommandList, TexResource, UploadBuffer, 0, 0, SubresourceSize, &SubresourceData[0]);
        auto Transition2 = CD3DX12_RESOURCE_BARRIER::Transition(TexResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        m_CommandList->ResourceBarrier(1, &Transition2);
        m_CommandList->Close();

        ID3D12CommandList *CommandLists[] = { m_CommandList };
        m_CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);

        Fence();
        FenceWait();

        if (UploadBuffer)
        {
            UploadBuffer->Release();
        }

        CD3DX12_CPU_DESCRIPTOR_HANDLE SRVHandle = {};
        m_SRVDescriptorPool->Alloc(&SRVHandle, nullptr, 1);
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {
            .Format = TexDesc.Format,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {
                .MipLevels = TexDesc.MipLevels,
            }
        };
        m_Device->CreateShaderResourceView(TexResource, &SRVDesc, SRVHandle);

        // Init
        Result = new texture;
        {
            Result->m_Resource  = TexResource;
            Result->m_CPUHandle = SRVHandle;
            Result->m_RefCount  = 0;
        }

        m_TextureTable[FileNameString] = Result;
    }
    else
    {
        Result = m_TextureTable[FileNameString];
        Result->m_RefCount++;
    }

    return Result;
}

void resource_manager::Fence()
{
    m_FenceValue++;
    m_CommandQueue->Signal(m_Fence, m_FenceValue);
}

void resource_manager::FenceWait()
{
    UINT64 ExpectedValue = m_FenceValue;
    if (m_Fence->GetCompletedValue() < ExpectedValue) 
    {
        m_Fence->SetEventOnCompletion(ExpectedValue, m_FenceEvent);
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }
}

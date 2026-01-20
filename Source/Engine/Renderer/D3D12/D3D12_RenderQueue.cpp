// Copyright Seong Woo Lee. All Rights Reserved.

void render_queue::Init(u32 MaxCount)
{
    m_MaxCount = MaxCount;
    m_Items = new render_item[MaxCount];
}

void render_queue::Push(render_item Item)
{
    ASSERT(m_Count < m_MaxCount);

    render_item* Entry = &m_Items[m_Count];
    *Entry = Item;

    m_Count++;
}

void render_queue::Process(command_list_pool* CommandListPool,
                           ID3D12CommandQueue* CommandQueue,
                           descriptor_pool* DescriptorPool,
                           constant_buffer_pool* ConstantBufferPool,
                           UINT MaxExecuteCountPerCommandList)
{
    const UINT MaxCommandListCount = 16;
    UINT CommandListCount = 0;
    ID3D12CommandList* CommandLists[MaxCommandListCount];
    UINT CurrentExecutedCountPerCommandList = 0;
    command_list* Node = CommandListPool->GetCurrent();
    ID3D12GraphicsCommandList* CommandList = Node->CommandList;

    CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12->m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), d3d12->m_RenderTargetIndex, d3d12->m_RTVDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle(d3d12->m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
    CommandList->RSSetViewports(1, &d3d12->m_Viewport);
    CommandList->RSSetScissorRects(1, &d3d12->m_ScissorRect);
    CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, &DSVHandle);

    for (u32 i = 0; i < m_Count; ++i)
    {
        render_item* Item = &m_Items[i];
        switch(Item->m_Type)
        {
            case RENDER_ITEM_MESH:
            {
                auto Data = Item->m_MeshData;
                mesh* Mesh = Data.m_Mesh;
                Mesh->Draw(CommandList, DescriptorPool, ConstantBufferPool, Data.m_WorldMatrix);
            } break;

            default:
            {
                ASSERT( !"Invalid default case." );
            } break;
        }

        if (++CurrentExecutedCountPerCommandList >= MaxExecuteCountPerCommandList)
        {
            CurrentExecutedCountPerCommandList = 0;

            ASSERT( CommandListCount < MaxCommandListCount );
            CommandLists[CommandListCount++] = CommandList;

            Node = CommandListPool->CloseAndProceed();
            CommandList = Node->CommandList;

            CommandList->RSSetViewports(1, &d3d12->m_Viewport);
            CommandList->RSSetScissorRects(1, &d3d12->m_ScissorRect);
            CommandList->OMSetRenderTargets(1, &RTVHandle, FALSE, &DSVHandle);
        }
    }

    // Push remainder.
    if (CurrentExecutedCountPerCommandList > 0)
    {
        ASSERT( CommandListCount < MaxCommandListCount );
        CommandLists[CommandListCount++] = CommandList;
        CommandListPool->CloseAndProceed();
    }

    // Actually execute.
    if (CommandListCount > 0)
    {
        CommandQueue->ExecuteCommandLists(CommandListCount, CommandLists);
    }

    // Clear count.
    m_Count = 0;
}

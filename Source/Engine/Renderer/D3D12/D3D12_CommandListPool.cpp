// Copyright Seong Woo Lee. All Rights Reserved.


void command_list_pool::Init(ID3D12Device5* Device, D3D12_COMMAND_LIST_TYPE Type)
{
    m_Device = Device;
    m_Type = Type;

    m_Current = Alloc();
}

command_list* command_list_pool::Alloc()
{
    command_list* Result = FirstFree;

    if (Result)
    {
        sll_pop_front(FirstFree, LastFree);
        Result->next = nullptr;
    }
    else
    {
        ID3D12CommandAllocator* CommandAllocator;
        ID3D12GraphicsCommandList* CommandList;
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator)) );
        ASSERT_SUCCEEDED( d3d12->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator, nullptr, IID_PPV_ARGS(&CommandList)) );

        Result = new command_list;
        Result->CommandAllocator = CommandAllocator;
        Result->CommandList = CommandList;
    }

    sll_push_back(First, Last, Result);
    return Result;
}

void command_list_pool::Reset()
{
    command_list* NextNode = nullptr;
    for (command_list* Node = First;
         Node != NULL;
         Node = NextNode)
    {
        NextNode = Node->next;

        if (!Node->Closed)
        {
            Node->Closed = true;
            Node->CommandList->Close();
        }

        Node->CommandAllocator->Reset();
        Node->CommandList->Reset(Node->CommandAllocator, nullptr);
        Node->Closed = false;

        sll_pop_front(First, Last);
        sll_push_back(FirstFree, LastFree, Node);
    }

    m_Current = Alloc();
}

command_list* command_list_pool::GetCurrent()
{
    // Speced to return non-null.
    return m_Current;
}

command_list* command_list_pool::Next()
{
    command_list* Result = m_Current->next;
    if (!Result)
    {
        Result = Alloc();
    }
    m_Current = Result;
    return Result;
}

command_list* command_list_pool::CloseAndProceed()
{
    ASSERT( !m_Current->Closed );

    m_Current->Closed = true;
    m_Current->CommandList->Close();

    return Next();
}

command_list* command_list_pool::CloseAndSubmit()
{
    ASSERT( !m_Current->Closed );

    m_Current->Closed = true;
    m_Current->CommandList->Close();

    d3d12->m_CommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&m_Current->CommandList);

    return Next();
}

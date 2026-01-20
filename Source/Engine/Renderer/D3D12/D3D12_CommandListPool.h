// Copyright Seong Woo Lee. All Rights Reserved.

struct command_list
{
    command_list* next = nullptr;
    ID3D12CommandAllocator* CommandAllocator = nullptr;
    ID3D12GraphicsCommandList* CommandList = nullptr;
    b32 Closed = false;
};

class command_list_pool
{
    public:
        void Init(ID3D12Device5* Device, D3D12_COMMAND_LIST_TYPE Type);
        command_list* Alloc();
        void Reset();
        command_list* GetCurrent();
        command_list* CloseAndProceed();
        command_list* CloseAndSubmit();

    private:
        command_list* Next();

        ID3D12Device5* m_Device = nullptr;
        D3D12_COMMAND_LIST_TYPE m_Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        command_list* m_Current = nullptr;

        command_list* FirstFree = nullptr;
        command_list* LastFree = nullptr;

        command_list* First = nullptr;
        command_list* Last = nullptr;
};

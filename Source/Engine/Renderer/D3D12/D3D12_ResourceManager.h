// Copyright Seong Woo Lee. All Rights Reserved.

class resource_manager
{
    public:
        void Init(ID3D12Device5* Device);
        void CreateVertexBuffer(UINT VertexSize, UINT VerticesCount, void* Vertices, D3D12_VERTEX_BUFFER_VIEW* OutVertexBufferView, ID3D12Resource** OutBuffer);
        void CreateIndexBuffer(UINT IndexSize, UINT IndicesCount, void* Indices, D3D12_INDEX_BUFFER_VIEW* OutIndexBufferView, ID3D12Resource** OutBuffer);
        struct texture* CreateTextureFromFileName(const WCHAR* FileName, UINT Length);

        void Fence();
        void FenceWait();

    private:
        ID3D12Device5* m_Device = nullptr;
        ID3D12CommandQueue* m_CommandQueue = nullptr;
        ID3D12CommandAllocator* m_CommandAllocator = nullptr;
        ID3D12GraphicsCommandList* m_CommandList = nullptr;
        std::unordered_map<std::wstring, texture*> m_TextureTable = {};

        descriptor_pool* m_SRVDescriptorPool = nullptr;

        ID3D12Fence* m_Fence = nullptr;
        HANDLE m_FenceEvent = {};
        UINT64 m_FenceValue = 0;
};

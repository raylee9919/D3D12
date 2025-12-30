// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

#define SWAP_CHAIN_FRAME_COUNT 2
#define MAX_DRAW_COUNT         2

struct Vertex {
    Vec3 Position;
    Vec4 Color;
    Vec2 UV;
};

struct Constant_Buffer {
    Vec2 Offset;
};

enum Descriptor_Index {
    DESCRIPTOR_INDEX_CBV      = 0,
    DESCRIPTOR_INDEX_TEX      = 1,
    DESCRIPTOR_INDEX_COUNT    = 2,
};

class D3D12 {
    public:
        D3D12(HWND Hwnd);
        ~D3D12();

        void    Begin();
        void    End();
        void    Present();

        UINT64  Fence();
        void    WaitForFenceValue();

        void    LoadAssets();

        void    InitRootSignature();
        void    CreateVertexBuffer(UINT32 VertexCount, Vertex *Vertices);
        void    CreateIndexBuffer(UINT32 IndexCount, UINT16 *Indices);
        void    CreateTexture(UINT32 width, UINT32 height, DXGI_FORMAT Format, UINT32 *data);
        void    CreateConstantBuffer();

    private:
        HWND                        m_Hwnd              = 0;
        DXGI_ADAPTER_DESC1          m_AdapterDesc       = {};
        ID3D12Device5*              m_Device            = nullptr;
        IDXGISwapChain3*            m_SwapChain         = nullptr;

        FLOAT                       m_AspectRatio       = 1.f;
        CD3DX12_VIEWPORT            m_Viewport          = {};
        CD3DX12_RECT                m_ScissorRect       = {};

        ID3D12CommandQueue*         m_CommandQueue      = nullptr;
        ID3D12CommandAllocator*     m_CommandAllocator  = nullptr;
        ID3D12GraphicsCommandList*  m_CommandList       = nullptr;

        ID3D12DescriptorHeap*       m_RTVHeap           = nullptr;
        UINT                        m_RTVDescriptorSize = 0;

        Descriptor_Pool *m_DescriptorPool = nullptr;

        UINT                        m_RenderTargetIndex = 0;
        ID3D12Resource*             m_RenderTargets[SWAP_CHAIN_FRAME_COUNT] = {};

        ID3D12Fence*                m_Fence             = nullptr;
        HANDLE                      m_FenceEvent        = {};
        UINT64                      m_FenceValue        = 0;

        ID3D12RootSignature        *m_RootSignature     = nullptr;

        ID3D12Resource             *m_VertexBuffer      = nullptr;
        D3D12_VERTEX_BUFFER_VIEW    m_VertexBufferView  = {};

        ID3D12Resource             *m_IndexBuffer       = nullptr;
        D3D12_INDEX_BUFFER_VIEW     m_IndexBufferView   = {};

        ID3D12Resource             *m_ConstantBuffer    = nullptr;

        ID3D12PipelineState        *m_PipelineState     = nullptr;

        ID3D12Resource             *m_Texture           = nullptr;


        Constant_Buffer *m_SysMappedConstantBuffer = nullptr;
};

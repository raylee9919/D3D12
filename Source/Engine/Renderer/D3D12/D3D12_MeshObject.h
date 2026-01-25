// Copyright Seong Woo Lee. All Rights Reserved.

struct vertex 
{
    Vec3 Position;
    Vec4 Color;
    Vec2 TexCoord;
};

struct texture
{
    ID3D12Resource *m_Resource = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {};
    u32 m_RefCount = 0;
};

enum
{
    MESH_DESCRIPTOR_INDEX_PER_OBJ_CBV   = 0,
    MESH_DESCRIPTOR_INDEX_PER_GROUP_SRV = 1,

    MESH_DESCRIPTOR_INDEX_COUNT
};

struct submesh
{
    UINT m_IndexCount = 0;
    ID3D12Resource *m_IndexBuffer = nullptr;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};
    texture *m_Texture = nullptr;
};

class d3d12_mesh : public i_mesh
{
    public:
        // @Temporary
        static const UINT g_DescriptorCountPerMesh    = 1; // 1 csv
        static const UINT g_DescriptorCountPerSubmesh = 1; // 1 srv

        ID3D12RootSignature* m_RootSignature = nullptr;
        ID3D12PipelineState* m_PipelineState = nullptr;

        ID3D12Resource* m_VertexBuffer = nullptr;
        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

        ID3D12Resource* m_IndexBuffer = nullptr;
        D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};

        std::vector<submesh*> m_Submesh = {};
        UINT m_SubmeshCount = 0;

        void Draw(ID3D12GraphicsCommandList* CommandList, descriptor_pool* DescriptorPool,
                  constant_buffer_pool* ConstantBufferPool, M4x4 Transform);

        void DrawSkinnedMesh(ID3D12GraphicsCommandList* CommandList, descriptor_pool* DescriptorPool,
                             constant_buffer_pool* ConstantBufferPool,
                             M4x4 Transform, M4x4* SkinningMatrices, u32 MatricesCount);

    private:
};

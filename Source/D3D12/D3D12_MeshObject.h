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
};

enum
{
    MESH_DESCRIPTOR_INDEX_PER_OBJ_CBV   = 0,
    MESH_DESCRIPTOR_INDEX_PER_GROUP_SRV = 1,

    MESH_DESCRIPTOR_INDEX_COUNT
};

struct index_group
{
    UINT m_IndexCount = 0;
    ID3D12Resource *m_IndexBuffer = nullptr;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView = {};
    texture *m_Texture = nullptr;
};

struct mesh_object
{
    // @Temporary
    static const UINT g_DescriptorCountPerMeshObject = 1; // 1 csv
    static const UINT g_DescriptorCountPerIndexGroup = 1; // 1 srv

    ID3D12RootSignature *m_RootSignature = nullptr;
    ID3D12PipelineState *m_PipelineState = nullptr;

    ID3D12Resource *m_VertexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView = {};

    std::vector<index_group *> m_IndexGroup = {};
    UINT m_IndexGroupCount = 0;


    void Draw(M4x4 Transform);
};

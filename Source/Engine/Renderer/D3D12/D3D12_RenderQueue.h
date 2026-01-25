// Copyright Seong Woo Lee. All Rights Reserved.

enum render_item_type
{
    RENDER_ITEM_INVALID       = 0,
    RENDER_ITEM_MESH          = 1,
    RENDER_ITEM_SKINNED_MESH  = 2,
};

struct render_item_mesh_data
{
    d3d12_mesh* m_Mesh;
    M4x4 m_WorldMatrix;
    Vec3 CameraPosition;
};

struct render_item_skinned_mesh
{
    d3d12_mesh* Mesh;
    M4x4        WorldMatrix;
    Vec3        CameraPosition;
    M4x4*       SkinningMatrices;
    u32         MatricesCount;
};

struct render_item
{
    render_item_type m_Type;
    union
    {
        render_item_mesh_data       m_MeshData;
        render_item_skinned_mesh    SkinnedMesh;
    };
};

class render_queue
{
    public:
        void Init(u32 MaxItemCount);
        void Push(render_item Item);
        void Process(CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle,
                     CD3DX12_CPU_DESCRIPTOR_HANDLE DSVHandle,
                     command_list_pool* CommandListPool,
                     ID3D12CommandQueue* CommandQueue,
                     descriptor_pool* DescriptorPool,
                     constant_buffer_pool* ConstantBufferPool,
                     UINT MaxExecuteCountPerCommandList);

    //private:
        render_item* m_Items = nullptr;
        u32 m_Count = 0;
        u32 m_MaxCount = 0;
};

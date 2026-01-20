// Copyright Seong Woo Lee. All Rights Reserved.

enum render_item_type
{
    RENDER_ITEM_INVALID = 0,
    RENDER_ITEM_MESH    = 1,
};

struct render_item_mesh_data
{
    mesh* m_Mesh;
    M4x4  m_WorldMatrix;
};

struct render_item
{
    render_item_type m_Type;
    union
    {
        render_item_mesh_data m_MeshData;
    };
};

class render_queue
{
    public:
        void Init(u32 MaxItemCount);
        void Push(render_item Item);
        void Execute(command_list_pool* CommandListPool, ID3D12CommandQueue* CommandQueue, UINT MaxExecuteCountPerCommandList);

    //private:
        render_item* m_Items = nullptr;
        u32 m_Count = 0;
        u32 m_MaxCount = 0;
};

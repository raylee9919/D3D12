// Copyright Seong Woo Lee. All Rights Reserved.

class i_renderer
{
    public:
        virtual i_mesh* CreateMesh(void* Vertices, u32 VertexSize, u32 VerticesCount, void* Indices, u32 IndexSize, u32 IndexCount) = 0;
};

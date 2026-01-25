// Copyright Seong Woo Lee. All Rights Reserved.

struct vert
{
    Vec3 Position;
    Vec3 Normal;

    f32 Weights[4];
    s32 Joints[4];
};
struct asset_mesh
{
    vert* Vertices;
    u32 VerticesCount;

    u16* Indices;
    u32 IndicesCount;
};

class ascii_loader
{
    public:
        std::pair<asset_mesh*, skeleton*> LoadMeshFromFilePath(const char* FilePath);
        animation_clip* LoadAnimationFromFilePath(const char* FilePath);
    private:
        void InitAndReadEntireFile(const char* FilePath);
        void Cleanup();
        char Peek();
        char Eat();
        b32 IsNum(char C);
        b32 IsWhitespace(char C);
        void EatWhitespaces();
        u16 ParseU16();
        u32 ParseU32();
        s32 ParseS32();
        f32 ParseF32();
        Vec3 ParseF32x3();
        Vec4 ParseF32x4();
        quaternion ParseQuaternion();
        M4x4 ParseMat4();
        std::string ParseString();
        u16 ParseVersion();
        u32 ParseCount(const char* What);
        void ParseJoints(joint* Joints, u32 Count);
        void ParseVertices(vert* Vertices, u32 Count);
        void ParseTriangles(u16* Indices, u32 TriangleCount);

        u8* m_Buffer    = nullptr;
        u8* m_BufferEnd = nullptr;
        u8* m_Cursor    = nullptr;
};

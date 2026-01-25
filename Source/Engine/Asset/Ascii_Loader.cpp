// Copyright Seong Woo Lee. All Rights Reserved.

std::pair<asset_mesh*, skeleton*> ascii_loader::LoadMeshFromFilePath(const char* FilePath)
{
    InitAndReadEntireFile(FilePath);

    u16 Version = ParseVersion();
    (void)Version;

    asset_mesh* Mesh   = new asset_mesh;
    skeleton* Skeleton = new skeleton;

    Skeleton->JointsCount = ParseCount("joint_count");
    Mesh->VerticesCount   = ParseCount("vertex_count");

    u32 TriCount = ParseCount("triangle_count");
    Mesh->IndicesCount   = TriCount*3;
    Mesh->Vertices       = new vert[Mesh->VerticesCount];
    Mesh->Indices        = new u16[Mesh->IndicesCount];
    Skeleton->Joints     = new joint[Skeleton->JointsCount];

    ParseJoints(Skeleton->Joints, Skeleton->JointsCount);
    ParseVertices(Mesh->Vertices, Mesh->VerticesCount);
    ParseTriangles(Mesh->Indices, TriCount);

    EatWhitespaces();
    ASSERT(m_Cursor == m_BufferEnd);

    Cleanup();

    return std::pair(Mesh, Skeleton);
}

animation_clip* ascii_loader::LoadAnimationFromFilePath(const char* FilePath)
{
    animation_clip* Result = nullptr;
    
    InitAndReadEntireFile(FilePath);

    u16 Version = ParseVersion();
    (void)Version;

    Result = new animation_clip;
    Result->JointsCount   = ParseCount("joint_count");
    Result->SamplesCount  = ParseCount("sample_count");
    Result->Samples       = new sample[Result->JointsCount*Result->SamplesCount];

    for (u32 JointIndex = 0; JointIndex < Result->JointsCount; ++JointIndex)
    {
        EatWhitespaces();
        std::string Name = ParseString();

        for (u32 SampleIndex = 0; SampleIndex < Result->SamplesCount; ++SampleIndex)
        {
            auto* Sample = Result->Samples + JointIndex*Result->SamplesCount + SampleIndex;

            EatWhitespaces();
            Sample->Translation = ParseF32x3();

            EatWhitespaces();
            Sample->Rotation = ParseQuaternion();

            EatWhitespaces();
            Sample->Scaling = ParseF32x3();
        }
    }

    EatWhitespaces();
    ASSERT(m_Cursor == m_BufferEnd);

    Cleanup();

    return Result;
}

void ascii_loader::InitAndReadEntireFile(const char* FilePath)
{
    FILE* File = fopen(FilePath, "rb");
    if (File)
    {
        fseek(File, 0, SEEK_END);
        u64 Size = ftell(File);
        fseek(File, 0, SEEK_SET);

        m_Cursor = m_Buffer = new u8[Size];
        fread(m_Buffer, Size, 1, File);
        m_BufferEnd = m_Buffer + Size;
        fclose(File);
    }
    else
    {
        ASSERT( !"Could not open file." );
    }
}

void ascii_loader::Cleanup()
{
    ASSERT( m_Buffer );
    delete m_Buffer;
    m_Buffer    = nullptr;
    m_BufferEnd = nullptr;
    m_Cursor    = nullptr;
}

char ascii_loader::Peek()
{
    ASSERT( m_Cursor && m_Cursor < m_BufferEnd );
    return *m_Cursor;
}

char ascii_loader::Eat()
{
    ASSERT( m_Cursor && m_Cursor < m_BufferEnd );
    char Result = *m_Cursor++;
    return Result;
}

b32 ascii_loader::IsNum(char C)
{
    if (C >= '0' && C <= '9') { return true; }
    return false;
}

b32 ascii_loader::IsWhitespace(char C)
{
    if (C == ' ' || C == '\r' || C == '\n' || C == '\t' || C == '\f' || C == '\v') { return true; }
    return false;
}

std::string ascii_loader::ParseString()
{
    u8* Start = m_Cursor;
    while (m_Cursor < m_BufferEnd)
    {
        char C = Peek();
        if (C != '\0' && !IsWhitespace(C)) {
            m_Cursor++;
        } else {
            break;
        }
    }
    return std::string(Start, m_Cursor);
}

void ascii_loader::EatWhitespaces()
{
    while (m_Cursor < m_BufferEnd)
    {
        u8 C = *m_Cursor;
        if (!IsWhitespace(C)) { break; }
        m_Cursor++;
    }
}

u16 ascii_loader::ParseU16()
{
    ASSERT( m_Cursor && m_Cursor < m_BufferEnd );
    u16 Result = 0;

    while (m_Cursor < m_BufferEnd)
    {
        char C = Peek();
        if (IsNum(C))
        {
            u16 Num = C - '0';
            Result *= 10;
            Result += Num;
            m_Cursor++;
        }
        else
        {
            break;
        }
    }

    return Result;
}

u32 ascii_loader::ParseU32()
{
    ASSERT( m_Cursor && m_Cursor < m_BufferEnd );
    u32 Result = 0;

    while (m_Cursor < m_BufferEnd)
    {
        char C = Peek();
        if (IsNum(C))
        {
            u32 Num = C - '0';
            Result *= 10;
            Result += Num;
            m_Cursor++;
        }
        else
        {
            break;
        }
    }

    return Result;
}

s32 ascii_loader::ParseS32()
{
    ASSERT( m_Cursor && m_Cursor < m_BufferEnd );

    b32 Signed = false;
    char SignChar = Peek();
    if (SignChar == '+') {
        Eat();
    } else if (SignChar == '-') {
        Signed = true;
        Eat();
    }

    u32 Integer = ParseU32();
    ASSERT( Integer <= 0x0fffffff );
    s32 Result = (u32)Integer;

    if (Signed) {
        Result = -Result;
    }

    return Result;
}

f32 ascii_loader::ParseF32()
{
    ASSERT( m_Cursor && m_Cursor < m_BufferEnd );

    b32 Signed = false;
    char SignChar = Peek();
    if (SignChar == '+') {
        Eat();
    } else if (SignChar == '-') {
        Signed = true;
        Eat();
    }

    s32 Integer = 0;
    while (m_Cursor < m_BufferEnd)
    {
        char C = Peek();
        if (IsNum(C))
        {
            s32 Num = C - '0';
            Integer *= 10;
            Integer += Num;
            m_Cursor++;
        }
        else
        {
            break;
        }
    }

    f32 Fraction = 0.f;
    f32 Weight = 0.1f;
    if (Peek() == '.')
    {
        m_Cursor++;

        while (m_Cursor < m_BufferEnd)
        {
            char C = Peek();
            if (IsNum(C))
            {
                f32 Num = (f32)(C - '0');
                Fraction += (Num*Weight);
                Weight *= 0.1f;
                m_Cursor++;
            }
            else
            {
                break;
            }
        }
    }

    f32 Result = (f32)Integer + Fraction;
    if (Signed) {
        Result = -Result;
    }
    return Result;
}

Vec3 ascii_loader::ParseF32x3()
{
    Vec3 Result;
    for (int i = 0; i < 3; ++i)
    {
        EatWhitespaces();
        Result.E[i] = ParseF32();
    }
    return Result;
}

Vec4 ascii_loader::ParseF32x4()
{
    Vec4 Result;
    for (int i = 0; i < 4; ++i)
    {
        EatWhitespaces();
        Result.E[i] = ParseF32();
    }
    return Result;
}

quaternion ascii_loader::ParseQuaternion()
{
    quaternion Result;

    EatWhitespaces();
    Result.X = ParseF32();

    EatWhitespaces();
    Result.Y = ParseF32();

    EatWhitespaces();
    Result.Z = ParseF32();

    EatWhitespaces();
    Result.W = ParseF32();

    return Result;
}

M4x4 ascii_loader::ParseMat4()
{
    M4x4 Result;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            EatWhitespaces();
            Result.E[r][c] = ParseF32();
        }
    }
    return Result;
}

u16 ascii_loader::ParseVersion()
{
    EatWhitespaces();

    u16 Version = 0;

    ASSERT( Eat() == '[' );
    while (IsNum(Peek()))
    {
        u16 Num = Peek() - '0';
        Version *= 10;
        Version += Num;
        m_Cursor++;
    }
    ASSERT( Eat() == ']' );
    return Version;
}

u32 ascii_loader::ParseCount(const char* What)
{
    EatWhitespaces();
    ASSERT( ParseString()==What );

    EatWhitespaces();
    u32 Count = 0;
    while (IsNum(Peek()))
    {
        u32 Num = Peek() - '0';
        Count *= 10;
        Count += Num;
        m_Cursor++;
    }

    return Count;
}

void ascii_loader::ParseJoints(joint* Joints, u32 Count)
{
    EatWhitespaces();
    ASSERT( ParseString()=="joints:" );

    for (u32 i = 0; i < Count; ++i)
    {
        joint* Joint = Joints + i;

        EatWhitespaces();
        Joint->Name = ParseString();

        EatWhitespaces();
        Joint->LocalTransform = ParseMat4();

        EatWhitespaces();
        Joint->Parent = ParseS32();

        if (Joint->Parent >= 0) {
            Joint->InverseRestPose = Joints[Joint->Parent].InverseRestPose * Joint->LocalTransform;
        }
        else {
            Joint->InverseRestPose = Joint->LocalTransform;
        }
    }

    for (u32 i = 0; i < Count; ++i)
    {
        joint* Joint = Joints + i;
        Joint->InverseRestPose = M4x4Inverse(Joint->InverseRestPose);
    }
}

void ascii_loader::ParseVertices(vert* Vertices, u32 Count)
{
    EatWhitespaces();
    ASSERT( ParseString()=="vertices:" );

    for (u32 i = 0; i < Count; ++i)
    {
        vert* Vert = Vertices + i;

        EatWhitespaces();
        Vert->Position = ParseF32x3();

        EatWhitespaces();
        Vert->Normal = ParseF32x3();

        f32 WeightSum = 0.f;
        for (int j = 0; j < 3; ++j) {
            EatWhitespaces();
            f32 Weight = ParseF32();
            Vert->Weights[j] = Weight;
            WeightSum += Weight;
        }
        ASSERT(WeightSum <= 1.f);
        Vert->Weights[3] = 1.0f - WeightSum;

        for (int j = 0; j < 4; ++j) {
            EatWhitespaces();
            Vert->Joints[j] = ParseS32();
        }
    }
}

void ascii_loader::ParseTriangles(u16* Indices, u32 TriangleCount)
{
    EatWhitespaces();
    ASSERT( ParseString()=="triangles:" );

    for (u32 i = 0; i < TriangleCount; ++i)
    {
        u16* Tri = Indices + 3*i;

        for (int j = 0; j < 3; ++j)
        {
            EatWhitespaces();
            Tri[j] = ParseU16();
        }
    }
}

// Copyright Seong Woo Lee. All Rights Reserved.

// ===========================================================
// Vec2
//
Vec2::Vec2(f32 X_, f32 Y_)
{
    X = X_;
    Y = Y_;
}
// ===========================================================
// Vec3
//
Vec3::Vec3(f32 X_, f32 Y_, f32 Z_)
{
    X = X_;
    Y = Y_;
    Z = Z_;
}

Vec3 operator+(Vec3 A, Vec3 B)
{
    Vec3 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;
    return Result;
}

Vec3 operator-(Vec3 A, Vec3 B)
{
    Vec3 Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;
    return Result;
}

Vec3 operator*(f32 F, Vec3 V)
{
    Vec3 Result;
    Result.X = F*V.X;
    Result.Y = F*V.Y;
    Result.Z = F*V.Z;
    return Result;
}

Vec3 operator*(Vec3 V, f32 F)
{
    Vec3 Result;
    Result.X = F*V.X;
    Result.Y = F*V.Y;
    Result.Z = F*V.Z;
    return Result;
}

Vec3 operator-(Vec3 &V)
{
    V.X = -V.X;
    V.Y = -V.Y;
    V.Z = -V.Z;
    return V;
}

f32 Dot(Vec3 A, Vec3 B)
{
    f32 Result = A.X*B.X + A.Y*B.Y + A.Z*B.Z;
    return Result;
}

Vec3 Cross(Vec3 A, Vec3 B)
{
    Vec3 Result;
    Result.X = A.Y*B.Z - A.Z*B.Y;
    Result.Y = A.Z*B.X - A.X*B.Z;
    Result.Z = A.X*B.Y - A.Y*B.X;
    return Result;
}

Vec3 Normalize(Vec3 V)
{
    Vec3 Result = {};
    f32 Len = sqrtf(V.X*V.X + V.Y*V.Y + V.Z*V.Z);
    if (Len != 0.f)
    {
        f32 InvLen = 1.f / Len;
        Result = InvLen*V;
    }
    return Result;
}

// ===========================================================
// Vec4
//
Vec4::Vec4(f32 X_, f32 Y_, f32 Z_, f32 W_)
{
    E[0] = X_;
    E[1] = Y_;
    E[2] = Z_;
    E[3] = W_;
}

f32 Dot(Vec4 A, Vec4 B)
{
    f32 Result = A.E[0]*B.E[0] + A.E[1]*B.E[1] + A.E[2]*B.E[2] + A.E[3]*B.E[3];
    return Result;
}

Vec4 operator*(Vec4 V, M4x4 M)
{
    Vec4 Result = {};
    Result.E[0] = Dot(V, M.Rows[0]);
    Result.E[1] = Dot(V, M.Rows[1]);
    Result.E[2] = Dot(V, M.Rows[2]);
    Result.E[3] = Dot(V, M.Rows[3]);
    return Result;
}

// ===========================================================
// M4x4
//
M4x4 M4x4Identity()
{
    M4x4 Result = {};
#if BUILD_USE_SSE
    Result.SSE[0] = _mm_setr_ps(1, 0, 0, 0);
    Result.SSE[1] = _mm_setr_ps(0, 1, 0, 0);
    Result.SSE[2] = _mm_setr_ps(0, 0, 1, 0);
    Result.SSE[3] = _mm_setr_ps(0, 0, 0, 1);
#else
    Result = {{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    }};
#endif
    return Result;
}

__m128 LinearCombineSSE(__m128 A, M4x4 B)
{
    __m128 Result;
    Result = _mm_mul_ps(_mm_shuffle_ps(A, A, 0x00), B.SSE[0]);
    Result = _mm_add_ps(Result, _mm_mul_ps(_mm_shuffle_ps(A, A, 0x55), B.SSE[1]));
    Result = _mm_add_ps(Result, _mm_mul_ps(_mm_shuffle_ps(A, A, 0xaa), B.SSE[2]));
    Result = _mm_add_ps(Result, _mm_mul_ps(_mm_shuffle_ps(A, A, 0xff), B.SSE[3]));
    return Result;
}

M4x4 operator*(M4x4 A, M4x4 B)
{
#if BUILD_USE_SSE
    M4x4 Result;
    __m128 Out0X = LinearCombineSSE(A.SSE[0], B);
    __m128 Out1X = LinearCombineSSE(A.SSE[1], B);
    __m128 Out2X = LinearCombineSSE(A.SSE[2], B);
    __m128 Out3X = LinearCombineSSE(A.SSE[3], B);

    Result.SSE[0] = Out0X;
    Result.SSE[1] = Out1X;
    Result.SSE[2] = Out2X;
    Result.SSE[3] = Out3X;
#else
    M4x4 Result = {};
    for (int R = 0; R < 4; ++R)
    {
        for (int C = 0; C < 4; ++C)
        {
            for (int I = 0; I < 4; ++I)
            {
                Result.E[R][C] += A.E[R][I]*B.E[I][C];
            }
        }
    }
#endif
    return Result;
}

M4x4 Transpose(M4x4 M)
{
#if BUILD_USE_SSE
    __m128 Temp1 = _mm_shuffle_ps(M.SSE[0], M.SSE[1], _MM_SHUFFLE(1, 0, 1, 0));
    __m128 Temp3 = _mm_shuffle_ps(M.SSE[0], M.SSE[1], _MM_SHUFFLE(3, 2, 3, 2));
    __m128 Temp2 = _mm_shuffle_ps(M.SSE[2], M.SSE[3], _MM_SHUFFLE(1, 0, 1, 0));
    __m128 Temp4 = _mm_shuffle_ps(M.SSE[2], M.SSE[3], _MM_SHUFFLE(3, 2, 3, 2));

    M4x4 Result;
    Result.SSE[0] = _mm_shuffle_ps(Temp1, Temp2, _MM_SHUFFLE(2, 0, 2, 0));
    Result.SSE[1] = _mm_shuffle_ps(Temp1, Temp2, _MM_SHUFFLE(3, 1, 3, 1));
    Result.SSE[2] = _mm_shuffle_ps(Temp3, Temp4, _MM_SHUFFLE(2, 0, 2, 0));
    Result.SSE[3] = _mm_shuffle_ps(Temp3, Temp4, _MM_SHUFFLE(3, 1, 3, 1));
    return Result;
#else
    M.e[0][1] = M.e[1][0];
    M.e[0][2] = M.e[2][0];
    M.e[0][3] = M.e[3][0];

    M.e[1][0] = M.e[0][1];
    M.e[1][2] = M.e[2][1];
    M.e[1][3] = M.e[3][1];

    M.e[2][0] = M.e[0][2];
    M.e[2][1] = M.e[1][2];
    M.e[2][3] = M.e[3][2];

    M.e[3][0] = M.e[0][3];
    M.e[3][1] = M.e[1][3];
    M.e[3][2] = M.e[2][3];
    return M;
#endif
}

M4x4 M4x4Translation(f32 X, f32 Y, f32 Z)
{
#if BUILD_USE_SSE
    M4x4 Result;
    Result.SSE[0] = _mm_setr_ps(1, 0, 0, X);
    Result.SSE[1] = _mm_setr_ps(0, 1, 0, Y);
    Result.SSE[2] = _mm_setr_ps(0, 0, 1, Z);
    Result.SSE[3] = _mm_setr_ps(0, 0, 0, 1);
    return Result;
#else
    M4x4 Result = {{
        { 1, 0, 0, X},
        { 0, 1, 0, Y},
        { 0, 0, 1, Z},
        { 0, 0, 0, 1},
    }};
    return Result;
#endif
}

M4x4 RotationX(f32 Angle)
{
    M4x4 Result = {};
    f32 C = cosf(Angle);
    f32 S = sinf(Angle);
    Result = {{
        { 1, 0, 0, 0},
        { 0, C,-S, 0},
        { 0, S, C, 0},
        { 0, 0, 0, 1},
    }};
    return Result;
}

M4x4 RotationY(f32 Angle)
{
    M4x4 Result = {};
    f32 C = cosf(Angle);
    f32 S = sinf(Angle);
    Result = {{
        { C, 0, S, 0},
        { 0, 1, 0, 0},
        {-S, 0, C, 0},
        { 0, 0, 0, 1},
    }};
    return Result;
}

M4x4 RotationZ(f32 Angle)
{
    M4x4 Result = {};
    f32 C = cosf(Angle);
    f32 S = sinf(Angle);
    Result = {{
        { C,-S, 0, 0},
        { S, C, 0, 0},
        { 0, 0, 1, 0},
        { 0, 0, 0, 1},
    }};
    return Result;
}

M4x4 M4x4LookToRH(Vec3 Eye, Vec3 Dir, Vec3 Up)
{
    Vec3 AxisX = Normalize(Cross(Dir, Up));
    Vec3 AxisY = Normalize(Cross(AxisX, Dir));
    Vec3 AxisZ = Dir;

    M4x4 Result = {};
    Result.E[0][0] = AxisX.X;
    Result.E[0][1] = AxisX.Y;
    Result.E[0][2] = AxisX.Z;
    Result.E[0][3] = -Dot(AxisX, Eye);
    Result.E[1][0] = AxisY.X;
    Result.E[1][1] = AxisY.Y;
    Result.E[1][2] = AxisY.Z;
    Result.E[1][3] = -Dot(AxisY, Eye);
    Result.E[2][0] = AxisZ.X;
    Result.E[2][1] = AxisZ.Y;
    Result.E[2][2] = AxisZ.Z;
    Result.E[2][3] = -Dot(AxisZ, Eye);
    Result.E[3][0] = 0.f;
    Result.E[3][1] = 0.f;
    Result.E[3][2] = 0.f;
    Result.E[3][3] = 1.f;
    return Result;
}

M4x4 M4x4LookAtRH(Vec3 Eye, Vec3 Focus, Vec3 Up)
{
    Vec3 Dir = Focus - Eye;
    return M4x4LookToRH(Eye, Dir, Up);
}

M4x4 M4x4PerspectiveLH(f32 Fov, f32 AspectRatio, f32 NearZ, f32 FarZ)
{
    // Z = [0,1]
    //
    ASSERT( NearZ > 0.f && FarZ > 0.f );

    const f32 F = 1.f / tanf(Fov*0.5f);
    const f32 A = AspectRatio;
    const f32 FmN = FarZ - NearZ;
    M4x4 Result = {{
        { F,   0,        0,                 0},
        { 0, F*A,        0,                 0},
        { 0,   0, FarZ/FmN, -(FarZ*NearZ)/FmN},
        { 0,   0,        1,                 0},
    }};
    return Result;
}

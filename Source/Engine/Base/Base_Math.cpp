// Copyright Seong Woo Lee. All Rights Reserved.

// ===========================================================
// Scalar
//
f32 Lerp(f32 A, f32 B, f32 T)
{
    return A + (B - A)*T;
}

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

Vec3 Lerp(Vec3 A, Vec3 B, f32 T)
{
    Vec3 Result;
    Result.E[0] = Lerp(A.E[0], B.E[0], T);
    Result.E[1] = Lerp(A.E[1], B.E[1], T);
    Result.E[2] = Lerp(A.E[2], B.E[2], T);
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
    Vec4 Result;
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

M4x4 M4x4Translation(Vec3 V)
{
    return M4x4Translation(V.X, V.Y, V.Z);
}

M4x4 M4x4Scale(Vec3 V)
{
    const f32 X = V.X;
    const f32 Y = V.Y;
    const f32 Z = V.Z;
#if BUILD_USE_SSE
    M4x4 Result;
    Result.SSE[0] = _mm_setr_ps(X, 0, 0, 0);
    Result.SSE[1] = _mm_setr_ps(0, Y, 0, 0);
    Result.SSE[2] = _mm_setr_ps(0, 0, Z, 0);
    Result.SSE[3] = _mm_setr_ps(0, 0, 0, 1);
#else
    M4x4 Result = {{
        { X, 0, 0, 0},
        { 0, Y, 0, 0},
        { 0, 0, Z, 0},
        { 0, 0, 0, 1},
    }};
#endif
    return Result;
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


M4x4 M4x4Inverse(M4x4 M)
{
    // @Todo: This is ugly.

    f32 A2323 = M.E[2][2] * M.E[3][3] - M.E[2][3] * M.E[3][2];
    f32 A1323 = M.E[2][1] * M.E[3][3] - M.E[2][3] * M.E[3][1];
    f32 A1223 = M.E[2][1] * M.E[3][2] - M.E[2][2] * M.E[3][1];
    f32 A0323 = M.E[2][0] * M.E[3][3] - M.E[2][3] * M.E[3][0];
    f32 A0223 = M.E[2][0] * M.E[3][2] - M.E[2][2] * M.E[3][0];
    f32 A0123 = M.E[2][0] * M.E[3][1] - M.E[2][1] * M.E[3][0];
    f32 A2313 = M.E[1][2] * M.E[3][3] - M.E[1][3] * M.E[3][2];
    f32 A1313 = M.E[1][1] * M.E[3][3] - M.E[1][3] * M.E[3][1];
    f32 A1213 = M.E[1][1] * M.E[3][2] - M.E[1][2] * M.E[3][1];
    f32 A2312 = M.E[1][2] * M.E[2][3] - M.E[1][3] * M.E[2][2];
    f32 A1312 = M.E[1][1] * M.E[2][3] - M.E[1][3] * M.E[2][1];
    f32 A1212 = M.E[1][1] * M.E[2][2] - M.E[1][2] * M.E[2][1];
    f32 A0313 = M.E[1][0] * M.E[3][3] - M.E[1][3] * M.E[3][0];
    f32 A0213 = M.E[1][0] * M.E[3][2] - M.E[1][2] * M.E[3][0];
    f32 A0312 = M.E[1][0] * M.E[2][3] - M.E[1][3] * M.E[2][0];
    f32 A0212 = M.E[1][0] * M.E[2][2] - M.E[1][2] * M.E[2][0];
    f32 A0113 = M.E[1][0] * M.E[3][1] - M.E[1][1] * M.E[3][0];
    f32 A0112 = M.E[1][0] * M.E[2][1] - M.E[1][1] * M.E[2][0];

    f32 det = (M.E[0][0] * ( M.E[1][1] * A2323 - M.E[1][2] * A1323 + M.E[1][3] * A1223 ) - 
               M.E[0][1] * ( M.E[1][0] * A2323 - M.E[1][2] * A0323 + M.E[1][3] * A0223 ) +
               M.E[0][2] * ( M.E[1][0] * A1323 - M.E[1][1] * A0323 + M.E[1][3] * A0123 ) -
               M.E[0][3] * ( M.E[1][0] * A1223 - M.E[1][1] * A0223 + M.E[1][2] * A0123 ));
    det = 1.f / det;

    M4x4 Result = M4x4{{
        det *   ( M.E[1][1] * A2323 - M.E[1][2] * A1323 + M.E[1][3] * A1223 ),
            det * - ( M.E[0][1] * A2323 - M.E[0][2] * A1323 + M.E[0][3] * A1223 ),
            det *   ( M.E[0][1] * A2313 - M.E[0][2] * A1313 + M.E[0][3] * A1213 ),
            det * - ( M.E[0][1] * A2312 - M.E[0][2] * A1312 + M.E[0][3] * A1212 ),
            det * - ( M.E[1][0] * A2323 - M.E[1][2] * A0323 + M.E[1][3] * A0223 ),
            det *   ( M.E[0][0] * A2323 - M.E[0][2] * A0323 + M.E[0][3] * A0223 ),
            det * - ( M.E[0][0] * A2313 - M.E[0][2] * A0313 + M.E[0][3] * A0213 ),
            det *   ( M.E[0][0] * A2312 - M.E[0][2] * A0312 + M.E[0][3] * A0212 ),
            det *   ( M.E[1][0] * A1323 - M.E[1][1] * A0323 + M.E[1][3] * A0123 ),
            det * - ( M.E[0][0] * A1323 - M.E[0][1] * A0323 + M.E[0][3] * A0123 ),
            det *   ( M.E[0][0] * A1313 - M.E[0][1] * A0313 + M.E[0][3] * A0113 ),
            det * - ( M.E[0][0] * A1312 - M.E[0][1] * A0312 + M.E[0][3] * A0112 ),
            det * - ( M.E[1][0] * A1223 - M.E[1][1] * A0223 + M.E[1][2] * A0123 ),
            det *   ( M.E[0][0] * A1223 - M.E[0][1] * A0223 + M.E[0][2] * A0123 ),
            det * - ( M.E[0][0] * A1213 - M.E[0][1] * A0213 + M.E[0][2] * A0113 ),
            det *   ( M.E[0][0] * A1212 - M.E[0][1] * A0212 + M.E[0][2] * A0112 ),
    }};

    return Result;
}

void M4x4Print(M4x4 M)
{
    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Col = 0; Col < 4; ++Col)
        {
            printf("%.2f ", M.E[Row][Col]);
        }
        printf("\n");
    }
    printf("\n");
}

// ===========================================================
// quaternion
//
quaternion operator+(quaternion A, quaternion B)
{
#if BUILD_USE_SSE
    quaternion Result;
    Result.SSE = _mm_add_ps(A.SSE, B.SSE);
    return Result;
#else
    quaternion Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;
    Result.W = A.W + B.W;
    return Result;
#endif
}

quaternion operator-(quaternion A, quaternion B)
{
#if BUILD_USE_SSE
    quaternion Result;
    Result.SSE = _mm_sub_ps(A.SSE, B.SSE);
    return Result;
#else
    quaternion Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;
    Result.W = A.W - B.W;
    return Result;
#endif
}

quaternion operator*(quaternion A, quaternion B)
{
    quaternion Result;
    Result.X = (A.W*B.X) + (A.X*B.W) + (A.Y*B.Z) - (A.Z*B.Y); 
    Result.Y = (A.W*B.Y) + (A.Y*B.W) + (A.Z*B.X) - (A.X*B.Z); 
    Result.Z = (A.W*B.Z) + (A.Z*B.W) + (A.X*B.Y) - (A.Y*B.X); 
    Result.W = (A.W*B.W) - (A.X*B.X) - (A.Y*B.Y) - (A.Z*B.Z); 
    return Result;
}

quaternion operator*(f32 F, quaternion Q)
{
#if BUILD_USE_SSE
    quaternion Result;
    __m128 F32x4 = _mm_set1_ps(F);
    Result.SSE = _mm_mul_ps(Q.SSE, F32x4);
    return Result;
#else
    quaternion Result;
    Result.X = Q.X*F;
    Result.Y = Q.Y*F;
    Result.Z = Q.Z*F;
    Result.W = Q.W*F;
    return Result;
#endif
}

quaternion operator*(quaternion Q, f32 F)
{
#if BUILD_USE_SSE
    quaternion Result;
    __m128 F32x4 = _mm_set1_ps(F);
    Result.SSE = _mm_mul_ps(Q.SSE, F32x4);
    return Result;
#else
    quaternion Result;
    Result.X = Q.X*F;
    Result.Y = Q.Y*F;
    Result.Z = Q.Z*F;
    Result.W = Q.W*F;
    return Result;
#endif
}

f32 Dot(quaternion A, quaternion B)
{
    f32 Result = A.W*B.W + A.X*B.X + A.Y*B.Y + A.Z*B.Z;
    return Result;
}

M4x4 M4x4Rotation(quaternion Q) 
{
    M4x4 M = {};

    f32 XS = Q.X * 2;
    f32 YS = Q.Y * 2;
    f32 ZS = Q.Z * 2;

    f32 WX = Q.W * XS;
    f32 WY = Q.W * YS;
    f32 WZ = Q.W * ZS;

    f32 _XX = Q.X * XS;
    f32 XY  = Q.X * YS;
    f32 XZ  = Q.X * ZS;

    f32 YY = Q.Y * YS;
    f32 YZ = Q.Y * ZS;
    f32 ZZ = Q.Z * ZS;

    M.E[0][0] = 1.0f - (YY + ZZ);
    M.E[0][1] = XY - WZ;
    M.E[0][2] = XZ + WY;

    M.E[1][0] = XY + WZ;
    M.E[1][1] = 1.0f - (_XX + ZZ);
    M.E[1][2] = YZ - WX;

    M.E[2][0] = XZ - WY;
    M.E[2][1] = YZ + WX;
    M.E[2][2] = 1.0f - (_XX + YY);

    M.E[3][3] = 1.0f;

    return M;
}

quaternion Lerp(quaternion A, quaternion B, f32 T)
{
    quaternion Result;
    Result.W = Lerp(A.W, B.W, T);
    Result.X = Lerp(A.X, B.X, T);
    Result.Y = Lerp(A.Y, B.Y, T);
    Result.Z = Lerp(A.Z, B.Z, T);
    return Result;
}

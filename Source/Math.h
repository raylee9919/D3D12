// Copyright Seong Woo Lee. All Rights Reserved.

struct Vec2 
{
    f32 X, Y;

    Vec2() = default;
    Vec2(f32 X_, f32 Y_);
};

struct Vec3 
{
    f32 X, Y, Z, Padding;

    Vec3() = default;
    Vec3(f32 X_, f32 Y_, f32 Z_);
};

union Vec4 
{
    f32 E[4];
    __m128 SSE;

    Vec4() = default;
    Vec4(f32 X_, f32 Y_, f32 Z_, f32 W_);
};

union M4x4
{
    f32 E[4][4];
    Vec4 Rows[4];
    __m128 SSE[4];
};

struct Quaternion
{
    f32 X, Y, Z;
    f32 W;
};

// ===========================================================
// Vec3
//
Vec3 operator+(Vec3 A, Vec3 B);
Vec3 operator-(Vec3 A, Vec3 B);
Vec3 operator*(f32  F, Vec3 V);
Vec3 operator*(Vec3 V, f32  F);
Vec3 operator-(Vec3 &V);
f32  Dot(Vec3 A, Vec3 B);
Vec3 Cross(Vec3 A, Vec3 B);
Vec3 Normalize(Vec3 V);

// ===========================================================
// Vec4
//
f32 Dot(Vec3 A, Vec3 B);
Vec4 operator*(Vec4 V, M4x4 M);

// ===========================================================
// M4x4
//
M4x4 M4x4Identity();
M4x4 operator*(M4x4 A, M4x4 B);
M4x4 Transpose(M4x4 M);
M4x4 M4x4Translation(f32 X, f32 Y, f32 Z);
M4x4 RotationX(f32 Angle);
M4x4 RotationY(f32 Angle);
M4x4 RotationZ(f32 Angle);
M4x4 M4x4LookToRH(Vec3 Eye, Vec3 Dir, Vec3 Up);
M4x4 M4x4LookAtRH(Vec3 Eye, Vec3 Focus, Vec3 Up);
M4x4 M4x4PerspectiveLH(f32 Fov/*Top-Down Y*/, f32 AspectRatio/*W divided by H*/, f32 NearZ, f32 FarZ);

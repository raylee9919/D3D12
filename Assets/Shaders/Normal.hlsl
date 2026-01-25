// Copyright Seong Woo Lee. All Rights Reserved.

cbuffer constant_buffer : register(b0)
{
    float4x4 g_World;
    float4x4 g_View;
    float4x4 g_Proj;
    float4x4 g_SkinningMatrices[128]; // @Todo: Sync.
};

struct vs_input
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    vector<float, 4> Weights : WEIGHTS;
    vector<int, 4> Joints: JOINTS;
};

struct ps_input
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
};

ps_input VSMain(vs_input Input)
{
    ps_input Result;

    float4 Position = float4(Input.Position, 1.0f);
    float4 SkinnedPosition = { 0.0f, 0.0f, 0.0f, 0.0f };

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (Input.Joints[i] >= 0) {
            float Weight = Input.Weights[i];
            float4x4 SkinningMatrix = g_SkinningMatrices[Input.Joints[i]];
            SkinnedPosition += mul(Position, SkinningMatrix) * Weight;
        }
        else {
            break;
        }
    }
    SkinnedPosition.w = 1.0f;

    Result.Position = mul(mul(mul(SkinnedPosition, g_World), g_View), g_Proj);
    Result.Normal = Input.Normal;

    return Result;
}

float4 PSMain(ps_input Input) : SV_TARGET
{
    float4 Result = float4(Input.Normal, 1.0f);
    return Result;
}

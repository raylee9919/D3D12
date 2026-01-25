// Copyright Seong Woo Lee. All Rights Reserved.

cbuffer constant_buffer : register(b0)
{
    float3   g_CameraPosition;
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
    float4 Normal = float4(Input.Normal, 0.0f);
    float3 SkinnedPosition = { 0.0f, 0.0f, 0.0f };
    float3 SkinnedNormal = { 0.0f, 0.0f, 0.0f };

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (Input.Joints[i] >= 0) {
            float Weight = Input.Weights[i];
            float4x4 SkinningMatrix = g_SkinningMatrices[Input.Joints[i]];

            float3 AddendPosition = (mul(Position, SkinningMatrix) * Weight).xyz;
            SkinnedPosition += AddendPosition;

            float3 AddendNormal = (mul(Normal, SkinningMatrix) * Weight).xyz;
            SkinnedNormal += AddendNormal; 
        } else {
            break;
        }
    }

    Result.Position = mul(mul(mul(float4(SkinnedPosition, 1.0f), g_World), g_View), g_Proj);
    Result.Normal = mul(SkinnedNormal, g_World);

    return Result;
}

float4 PSMain(ps_input Input) : SV_TARGET
{
    float3 LightPosition  = float3(0.0f, 3.0f, 0.0f);
    float3 LightIntensity = float3(1.0f, 1.0f, 1.0f);

    float3 ToLight = normalize(LightPosition - Input.Position);
    float3 ToEye   = normalize(g_CameraPosition - Input.Position);
    float3 Halfway = normalize(ToLight + ToEye);
    float3 Reflect = normalize(reflect(-ToLight, Input.Normal));

    float PhongDiffuseCoeff  = 0.5f;
    float PhongSpecularCoeff = 0.5f;
    float PhongShininess     = 16.0f;
    float3 PhongDiffuseTerm  = PhongDiffuseCoeff  * LightIntensity;
    float3 PhongSpecularTerm = PhongSpecularCoeff * LightIntensity * pow(dot(Reflect, ToEye), PhongShininess);
    float3 PhongResult       = PhongDiffuseTerm + PhongSpecularTerm;

    float4 Result = float4(PhongResult, 1.0f);
    return Result;
}

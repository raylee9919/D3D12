// Copyright Seong Woo Lee. All Rights Reserved.

Texture2D TexDiffuse : register(t0);
SamplerState SamplerDiffuse : register(s0);

cbuffer constant_buffer : register(b0)
{
    matrix g_World;
    matrix g_View;
    matrix g_Proj;
};

struct vs_input
{
    float4 Position : POSITION;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

struct ps_input
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};





ps_input VSMain(vs_input Input)
{
    ps_input Result;

    Result.Position = mul(mul(mul(Input.Position, g_World), g_View), g_Proj);
    Result.Color    = Input.Color;
    Result.TexCoord = Input.TexCoord;

    return Result;
}

float4 PSMain(ps_input Input) : SV_TARGET
{
    float4 TexColor = TexDiffuse.Sample(SamplerDiffuse, Input.TexCoord);
    return TexColor*Input.Color;
}

// Copyright Seong Woo Lee. All Rights Reserved.

Texture2D TexDiffuse : register(t0);
SamplerState SamplerDiffuse : register(s0);

cbuffer constant_buffer : register(b0)
{
    float2 gOffset;
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

    Result.Position = Input.Position + float4(gOffset, 0, 0);
    Result.Color    = Input.Color;
    Result.TexCoord = Input.TexCoord;

    return Result;
}

float4 PSMain(ps_input Input) : SV_TARGET
{
    float4 TexColor = TexDiffuse.Sample(SamplerDiffuse, Input.TexCoord);
    return TexColor*Input.Color;
}

// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

cbuffer Constant_Buffer : register(b0)
{
    float2 cOffset;
};
Texture2D TexDiffuse : register(t0);
SamplerState SamplerDiffuse : register(s0);


struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

PSInput VSMain(float4 Position : POSITION, float4 Color : COLOR, float2 UV : TEXCOORD)
{
    PSInput Result;

    Position.xy += cOffset;
    Result.Position = Position;
    Result.Color = Color;
    Result.UV = UV;

    return Result;
}

float4 PSMain(PSInput Input) : SV_TARGET
{
    float4 Result = TexDiffuse.Sample(SamplerDiffuse, Input.UV);
    Result *= Input.Color;
    return Result;
}

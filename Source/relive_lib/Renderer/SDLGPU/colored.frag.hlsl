struct FragmentInput
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR0;
};

Texture2D textureData : register(t0, space2);
SamplerState textureSampler : register(s0, space2);

float4 main(FragmentInput input) : SV_Target0
{
    return textureData.Sample(textureSampler, input.uv) * input.color;
}

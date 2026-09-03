struct PassthruVertexInput
{
    float2 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

struct PassthruVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer PassthruVertexUniforms : register(b0, space1)
{
    float2 viewportSize;
};

PassthruVertexOutput passthru_vertex(PassthruVertexInput input)
{
    PassthruVertexOutput output;
    output.position = float4(
        input.position.x / viewportSize.x * 2.0f - 1.0f,
        1.0f - input.position.y / viewportSize.y * 2.0f,
        0.0f,
        1.0f);
    output.uv = input.uv;
    return output;
}

Texture2D passthruTexture : register(t0, space2);
SamplerState passthruSampler : register(s0, space2);

cbuffer PassthruFragmentUniforms : register(b0, space3)
{
    float2 textureSize;
    uint flipUv;
};

float4 passthru_fragment(PassthruVertexOutput input) : SV_Target0
{
    float2 scaledUv = input.uv / textureSize;
    if (flipUv != 0)
    {
        scaledUv.y = 1.0f - scaledUv.y;
    }
    return passthruTexture.Sample(passthruSampler, scaledUv);
}

float3 get_normalized_from_888(uint value)
{
    return float3(
        ((value >> 16) & 0xFFu) / 255.0f,
        ((value >> 8) & 0xFFu) / 255.0f,
        (value & 0xFFu) / 255.0f);
}

uint get_888_from_normalized(float3 value)
{
    uint r = (uint)ceil(value.r * 255.0f);
    uint g = (uint)ceil(value.g * 255.0f);
    uint b = (uint)ceil(value.b * 255.0f);
    return (r << 16) | (g << 8) | b;
}

float4 passthru_filter_fragment(PassthruVertexOutput input) : SV_Target0
{
    float2 uv = input.uv / textureSize;
    if (input.position.y - floor(input.position.y / 2.0f) * 2.0f > 0.0f)
    {
        float4 above = passthruTexture.Sample(passthruSampler, float2(input.uv.x, input.uv.y + 1.0f) / textureSize);
        float4 below = passthruTexture.Sample(passthruSampler, uv);
        uint above888 = get_888_from_normalized(above.rgb);
        uint below888 = get_888_from_normalized(below.rgb);
        uint tmp1 = above888 & 0xF8F8F8u;
        uint tmp2 = below888 & 0xF8F8F8u;
        uint result = (((tmp1 + tmp2) >> 1) | ((tmp1 + tmp2) << 23)) & 0xFFFFFFu;
        return float4(get_normalized_from_888(result), 1.0f);
    }
    return passthruTexture.Sample(passthruSampler, uv);
}

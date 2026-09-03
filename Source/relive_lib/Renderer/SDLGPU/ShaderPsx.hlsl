struct PsxVertexInput
{
    float2 position : TEXCOORD0;
    float3 shadeColor : TEXCOORD1;
    float2 uv : TEXCOORD2;
    uint4 flags : TEXCOORD3;
    uint2 textureIndexing : TEXCOORD4;
};

struct PsxVertexOutput
{
    float4 position : SV_Position;
    float3 shadeColor : TEXCOORD0;
    float2 uv : TEXCOORD1;
    uint4 flags : TEXCOORD2;
    uint2 textureIndexing : TEXCOORD3;
};

cbuffer PsxVertexUniforms : register(b0, space1)
{
    float2 viewportSize;
};

PsxVertexOutput psx_vertex(PsxVertexInput input)
{
    PsxVertexOutput output;
    output.position = float4(
        input.position.x / viewportSize.x * 2.0f - 1.0f,
        1.0f - input.position.y / viewportSize.y * 2.0f,
        0.0f,
        1.0f);
    output.shadeColor = input.shadeColor;
    output.uv = input.uv;
    output.flags = input.flags;
    output.textureIndexing = input.textureIndexing;
    return output;
}

Texture2D paletteTexture : register(t0, space2);
Texture2D gasTexture : register(t1, space2);
Texture2D cameraTexture : register(t2, space2);
Texture2D spriteSheets[12] : register(t3, space2);
Texture2D framebufferTexture : register(t15, space2);
SamplerState psxSampler : register(s0, space2);

cbuffer PsxFragmentUniforms : register(b0, space3)
{
    float2 spriteSheetSize[12];
    uint drawingFramebuffer;
};

static const uint BLEND_MODE_HALF_DST_ADD_HALF_SRC = 0;
static const uint BLEND_MODE_ONE_DST_ADD_ONE_SRC = 1;
static const uint BLEND_MODE_ONE_DST_SUB_ONE_SRC = 2;
static const uint BLEND_MODE_ONE_DST_ADD_QRT_SRC = 3;

static const uint DRAW_FLAT = 0;
static const uint DRAW_DEFAULT_FT4 = 1;
static const uint DRAW_CAM = 2;
static const uint DRAW_FG1 = 3;
static const uint DRAW_GAS = 4;

static const float2 frameSize = float2(640.0f, 240.0f);

float4 pixel_to_palette(float value, uint paletteIndex)
{
    return paletteTexture.Sample(psxSampler, float2(value, (float)paletteIndex / 255.0f));
}

bool dither(float4 position)
{
    return fmod(position.x + fmod(position.y, 2.0f), 2.0f) > 0.0f;
}

float3 handle_shading(float3 texel, float3 shadeColor, uint4 flags)
{
    if (flags.z != 0)
    {
        return saturate((texel * (shadeColor / 255.0f)) / 0.5f);
    }
    return texel;
}

float4 handle_final_color(float4 source, float3 shadeColor, uint4 flags, bool doShading)
{
    if (all(source == 0.0f))
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float4 result = source;
    if (doShading)
    {
        result.rgb = handle_shading(source.rgb, shadeColor, flags);
    }

    if (flags.y != 0 && source.a == 1.0f)
    {
        if (flags.w == BLEND_MODE_HALF_DST_ADD_HALF_SRC)
        {
            result = float4(result.rgb * 0.5f, 0.5f);
        }
        else if (flags.w == BLEND_MODE_ONE_DST_ADD_ONE_SRC || flags.w == BLEND_MODE_ONE_DST_SUB_ONE_SRC)
        {
            result = float4(result.rgb, 1.0f);
        }
        else if (flags.w == BLEND_MODE_ONE_DST_ADD_QRT_SRC)
        {
            result = float4(result.rgb * 0.25f, 1.0f);
        }
    }
    else
    {
        result.a = 0.0f;
    }
    return result;
}

float4 sample_sprite(PsxVertexOutput input)
{
    int2 pixel = int2(input.uv);
    switch (input.textureIndexing.y)
    {
        case 0: return spriteSheets[0].Load(int3(pixel, 0));
        case 1: return spriteSheets[1].Load(int3(pixel, 0));
        case 2: return spriteSheets[2].Load(int3(pixel, 0));
        case 3: return spriteSheets[3].Load(int3(pixel, 0));
        case 4: return spriteSheets[4].Load(int3(pixel, 0));
        case 5: return spriteSheets[5].Load(int3(pixel, 0));
        case 6: return spriteSheets[6].Load(int3(pixel, 0));
        case 7: return spriteSheets[7].Load(int3(pixel, 0));
        case 8: return spriteSheets[8].Load(int3(pixel, 0));
        case 9: return spriteSheets[9].Load(int3(pixel, 0));
        case 10: return spriteSheets[10].Load(int3(pixel, 0));
        default: return spriteSheets[11].Load(int3(pixel, 0));
    }
}

float4 psx_fragment(PsxVertexOutput input) : SV_Target0
{
    if (drawingFramebuffer != 0)
    {
        return float4(framebufferTexture.Sample(psxSampler, input.uv / frameSize).rgb, 0.0f);
    }

    switch (input.flags.x)
    {
        case DRAW_FLAT:
            return handle_final_color(float4(input.shadeColor / 255.0f, 1.0f), input.shadeColor, input.flags, false);
        case DRAW_DEFAULT_FT4:
            return handle_final_color(pixel_to_palette(sample_sprite(input).r, input.textureIndexing.x), input.shadeColor, input.flags, true);
        case DRAW_CAM:
            return float4(cameraTexture.Sample(psxSampler, input.uv / frameSize).rgb, 0.0f);
        case DRAW_FG1:
        {
            float4 mask = sample_sprite(input);
            float4 result = float4(cameraTexture.Sample(psxSampler, input.uv / frameSize).rgb, 0.0f);
            if (all(mask.rgb == 0.0f))
            {
                result = float4(0.0f, 0.0f, 0.0f, 1.0f);
            }
            return result;
        }
        case DRAW_GAS:
        {
            float4 gas = gasTexture.Sample(psxSampler, input.uv / frameSize);
            return dither(input.position) ? gas * 0.5f : float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

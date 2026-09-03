#version 450

layout(location = 0) in vec3 shadeColor;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in uvec4 flags;
layout(location = 3) flat in uvec2 textureIndexing;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D paletteTexture;
layout(set = 2, binding = 1) uniform sampler2D cameraTexture;
layout(set = 2, binding = 2) uniform sampler2D spriteTexture;
layout(set = 2, binding = 3) uniform sampler2D gasTexture;
layout(set = 2, binding = 4) uniform sampler2D framebufferTexture;

layout(set = 3, binding = 0, std140) uniform FragmentUniforms
{
    vec2 spriteSheetSize[12];
    uint drawingFramebuffer;
};

const uint DRAW_DEFAULT_FT4 = 1u;
const uint DRAW_CAM = 2u;
const uint DRAW_FG1 = 3u;
const uint DRAW_GAS = 4u;
const uint DRAW_FRAMEBUFFER = 5u;

vec4 paletteColor(float index)
{
    return texture(paletteTexture, vec2(index, float(textureIndexing.x) / 255.0));
}

vec3 shaded(vec3 color)
{
    if (flags.z != 0u)
    {
        return clamp((color * (shadeColor / 255.0)) / 0.5, 0.0, 1.0);
    }
    return color;
}

vec4 finalColor(vec4 source, bool doShading)
{
    if (source == vec4(0.0))
    {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }

    vec4 result = source;
    if (doShading)
    {
        result.rgb = shaded(source.rgb);
    }

    if (flags.y != 0u && source.a == 1.0)
    {
        if (flags.w == 0u)
        {
            result = vec4(result.rgb * 0.5, 0.5);
        }
        else if (flags.w == 1u || flags.w == 2u)
        {
            result = vec4(result.rgb, 1.0);
        }
        else if (flags.w == 3u)
        {
            result = vec4(result.rgb * 0.25, 1.0);
        }
    }
    else
    {
        result.a = 0.0;
    }
    return result;
}

void main()
{
    if (flags.x == DRAW_FRAMEBUFFER)
    {
        outColor = texture(framebufferTexture, uv / vec2(640.0, 240.0));
    }
    else if (flags.x == DRAW_GAS)
    {
        vec4 gas = texture(gasTexture, uv / vec2(640.0, 240.0));
        if (mod(gl_FragCoord.x + mod(gl_FragCoord.y, 2.0), 2.0) > 0.0)
        {
            outColor = gas * 0.5;
        }
        else
        {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
    }
    else if (flags.x == DRAW_CAM)
    {
        outColor = vec4(texture(cameraTexture, uv / vec2(640.0, 240.0)).rgb, 0.0);
    }
    else if (flags.x == DRAW_FG1)
    {
        vec4 mask = texelFetch(spriteTexture, ivec2(uv), 0);
        outColor = vec4(texture(cameraTexture, uv / vec2(640.0, 240.0)).rgb, 0.0);
        if (mask.rgb == vec3(0.0))
        {
            outColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
    }
    else if (flags.x == DRAW_DEFAULT_FT4)
    {
        outColor = finalColor(paletteColor(texelFetch(spriteTexture, ivec2(uv), 0).r), true);
    }
    else
    {
        outColor = finalColor(vec4(shadeColor / 255.0, 1.0), false);
    }
}
#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 shadeColor;
layout(location = 2) in vec2 uv;
layout(location = 3) in uvec4 flags;
layout(location = 4) in uvec2 textureIndexing;

layout(set = 1, binding = 0, std140) uniform VertexUniforms
{
    vec2 viewportSize;
};

layout(location = 0) out vec3 outShadeColor;
layout(location = 1) out vec2 outUv;
layout(location = 2) flat out uvec4 outFlags;
layout(location = 3) flat out uvec2 outTextureIndexing;

void main()
{
    gl_Position = vec4(
        position.x / viewportSize.x * 2.0 - 1.0,
        1.0 - position.y / viewportSize.y * 2.0,
        0.0,
        1.0);
    outShadeColor = shadeColor;
    outUv = uv;
    outFlags = flags;
    outTextureIndexing = textureIndexing;
}
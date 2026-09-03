cbuffer VertexUniform : register(b0, space1)
{
    float2 viewportSize;
};

struct VertexInput
{
    [[vk::location(0)]] float2 position : TEXCOORD0;
    [[vk::location(1)]] float2 uv : TEXCOORD1;
    [[vk::location(2)]] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR0;
};

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = float4(
        (input.position.x / viewportSize.x) * 2.0f - 1.0f,
        1.0f - (input.position.y / viewportSize.y) * 2.0f,
        0.0f,
        1.0f);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

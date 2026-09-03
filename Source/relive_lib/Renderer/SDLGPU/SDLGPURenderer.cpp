#include "SDLGPURenderer.hpp"

#include "../../../relive_lib/Primitives.hpp"
#include "../../../relive_lib/Animation.hpp"
#include "../../../relive_lib/ResourceManagerWrapper.hpp"
#include "../../../relive_lib/data_conversion/rgb_conversion.hpp"
#include "../../../relive_lib/data_conversion/AnimationConverter.hpp"
#include "../../../AliveLibAE/Font.hpp"

#include "generated/ft4_vert_spv.hpp"
#include "generated/ft4_frag_spv.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace
{
SDL_GPUBuffer* CreateBuffer(SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage, Uint32 size)
{
    SDL_GPUBufferCreateInfo info = {};
    info.usage = usage;
    info.size = size;
    return SDL_CreateGPUBuffer(device, &info);
}

SDL_GPUTransferBuffer* CreateTransferBuffer(SDL_GPUDevice* device, Uint32 size)
{
    SDL_GPUTransferBufferCreateInfo info = {};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = size;
    return SDL_CreateGPUTransferBuffer(device, &info);
}

u32 Color(u8 r, u8 g, u8 b, u8 a = 255)
{
    return static_cast<u32>(r) | (static_cast<u32>(g) << 8) | (static_cast<u32>(b) << 16) | (static_cast<u32>(a) << 24);
}
}

SDLGPURenderer::SDLGPURenderer(TWindowHandleType window)
    : IRenderer(window), mContext(window)
{
    mBatches.reserve(64);
    mBatches.emplace_back();
    mRendererStartTicks = SDL_GetTicks();
    CreatePipeline();

    mVertexTransferBuffer = CreateTransferBuffer(mContext.Device(), 65536 * sizeof(Vertex));
    mIndexTransferBuffer = CreateTransferBuffer(mContext.Device(), 98304 * sizeof(u16));

    SDL_GPUTextureCreateInfo textureInfo = {};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    textureInfo.width = 1;
    textureInfo.height = 1;
    textureInfo.layer_count_or_depth = 1;
    textureInfo.num_levels = 1;
    textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    mWhiteTexture = SDL_CreateGPUTexture(mContext.Device(), &textureInfo);
    textureInfo.width = 256;
    textureInfo.height = 256;
    mPaletteTexture = SDL_CreateGPUTexture(mContext.Device(), &textureInfo);
    textureInfo.width = kPsxFramebufferWidth;
    textureInfo.height = kPsxFramebufferHeight;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    mCurrentFramebuffer = SDL_CreateGPUTexture(mContext.Device(), &textureInfo);
    mPreviousFramebuffer = SDL_CreateGPUTexture(mContext.Device(), &textureInfo);
    mGasTexture = std::make_shared<SDLGPUTexture>(mContext.Device(), kPsxFramebufferWidth, kPsxFramebufferHeight, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    mTextureTransferBuffer = CreateTransferBuffer(mContext.Device(), 256 * 256 * sizeof(RGBA32));
}

void SDLGPURenderer::CreatePipeline()
{
    SDL_GPUShaderCreateInfo vertexInfo = {};
    vertexInfo.code_size = sizeof(g_ft4_vert_spv);
    vertexInfo.code = g_ft4_vert_spv;
    vertexInfo.entrypoint = "main";
    vertexInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vertexInfo.num_uniform_buffers = 1;
    mVertexShader = SDL_CreateGPUShader(mContext.Device(), &vertexInfo);

    SDL_GPUShaderCreateInfo fragmentInfo = {};
    fragmentInfo.code_size = sizeof(g_ft4_frag_spv);
    fragmentInfo.code = g_ft4_frag_spv;
    fragmentInfo.entrypoint = "main";
    fragmentInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fragmentInfo.num_samplers = 5;
    fragmentInfo.num_uniform_buffers = 1;
    mFragmentShader = SDL_CreateGPUShader(mContext.Device(), &fragmentInfo);
    if (!mVertexShader || !mFragmentShader)
    {
        ALIVE_FATAL("SDL GPU shader creation failed: %s", SDL_GetError());
    }

    SDL_GPUSamplerCreateInfo samplerInfo = {};
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    mSampler = SDL_CreateGPUSampler(mContext.Device(), &samplerInfo);
    if (!mSampler)
    {
        ALIVE_FATAL("SDL GPU sampler creation failed: %s", SDL_GetError());
    }
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    mLinearSampler = SDL_CreateGPUSampler(mContext.Device(), &samplerInfo);
    if (!mLinearSampler)
    {
        ALIVE_FATAL("SDL GPU linear sampler creation failed: %s", SDL_GetError());
    }

    SDL_GPUVertexBufferDescription bufferDescription = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
    SDL_GPUVertexAttribute attributes[] = {
        { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, x) },
        { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, r) },
        { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, u) },
        { 3, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, offsetof(Vertex, drawMode) },
        { 4, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT2, offsetof(Vertex, paletteIndex) }
    };
    SDL_GPUVertexInputState vertexInput = {};
    vertexInput.vertex_buffer_descriptions = &bufferDescription;
    vertexInput.num_vertex_buffers = 1;
    vertexInput.vertex_attributes = attributes;
    vertexInput.num_vertex_attributes = 5;

    SDL_GPUColorTargetDescription colorTarget = {};
    colorTarget.format = mContext.SwapchainFormat();
    colorTarget.blend_state.enable_blend = true;
    colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.vertex_shader = mVertexShader;
    pipelineInfo.fragment_shader = mFragmentShader;
    pipelineInfo.vertex_input_state = vertexInput;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipelineInfo.target_info.color_target_descriptions = &colorTarget;
    pipelineInfo.target_info.num_color_targets = 1;
    mPipeline = SDL_CreateGPUGraphicsPipeline(mContext.Device(), &pipelineInfo);
    if (!mPipeline)
    {
        ALIVE_FATAL("SDL GPU pipeline creation failed: %s", SDL_GetError());
    }

    colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
    mSubtractPipeline = SDL_CreateGPUGraphicsPipeline(mContext.Device(), &pipelineInfo);
    if (!mSubtractPipeline)
    {
        ALIVE_FATAL("SDL GPU subtract pipeline creation failed: %s", SDL_GetError());
    }
}

SDLGPURenderer::~SDLGPURenderer()
{
    SDL_WaitForGPUIdle(mContext.Device());
    for (Batch& batch : mBatches)
    {
        SDL_ReleaseGPUBuffer(mContext.Device(), batch.vertexBuffer);
        SDL_ReleaseGPUBuffer(mContext.Device(), batch.indexBuffer);
    }
    SDL_ReleaseGPUTransferBuffer(mContext.Device(), mVertexTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(mContext.Device(), mIndexTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(mContext.Device(), mTextureTransferBuffer);
    SDL_ReleaseGPUTexture(mContext.Device(), mWhiteTexture);
    SDL_ReleaseGPUTexture(mContext.Device(), mPaletteTexture);
    SDL_ReleaseGPUTexture(mContext.Device(), mCurrentFramebuffer);
    SDL_ReleaseGPUTexture(mContext.Device(), mPreviousFramebuffer);
    SDL_ReleaseGPUSampler(mContext.Device(), mSampler);
    SDL_ReleaseGPUSampler(mContext.Device(), mLinearSampler);
    SDL_ReleaseGPUGraphicsPipeline(mContext.Device(), mPipeline);
    SDL_ReleaseGPUGraphicsPipeline(mContext.Device(), mSubtractPipeline);
    SDL_ReleaseGPUShader(mContext.Device(), mVertexShader);
    SDL_ReleaseGPUShader(mContext.Device(), mFragmentShader);
}

void SDLGPURenderer::StartFrame()
{
    IRenderer::StartFrame();
    mOffsetX = 0;
    mOffsetY = 0;
    mCameraTexture.reset();
    mPaletteRows.clear();
    mPalettePixels.fill({});
    mCommandBuffer = mContext.BeginFrame(&mSwapchainTexture);
    if (mHasPreviousFramebuffer)
    {
        AddFramebufferQuad(mPreviousFramebuffer);
    }
}

void SDLGPURenderer::Clear(u8 r, u8 g, u8 b)
{
    mClearR = r;
    mClearG = g;
    mClearB = b;
}

void SDLGPURenderer::AddTriangle(const Vertex& a, const Vertex& b, const Vertex& c)
{
    if (mBatches.back().texture || mBatches.back().framebufferTexture)
    {
        mBatches.emplace_back();
    }
    Batch& batch = mBatches.back();
    const u16 base = static_cast<u16>(batch.vertices.size());
    batch.vertices.insert(batch.vertices.end(), { a, b, c });
    batch.scissor = mClipRect;
    batch.indices.insert(batch.indices.end(), { base, static_cast<u16>(base + 1), static_cast<u16>(base + 2) });
}

void SDLGPURenderer::AddQuad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
    if (mBatches.back().texture || mBatches.back().framebufferTexture)
    {
        mBatches.emplace_back();
    }
    Batch& batch = mBatches.back();
    const u16 base = static_cast<u16>(batch.vertices.size());
    batch.vertices.insert(batch.vertices.end(), { a, b, c, d });
    batch.scissor = mClipRect;
    batch.indices.insert(batch.indices.end(), { base, static_cast<u16>(base + 1), static_cast<u16>(base + 2), static_cast<u16>(base + 1), static_cast<u16>(base + 2), static_cast<u16>(base + 3) });
}

void SDLGPURenderer::AddLineSegment(f32 x0, f32 y0, f32 x1, f32 y1, u8 r0, u8 g0, u8 b0, u8 r1, u8 g1, u8 b1, bool semiTransparent, relive::TBlendModes blendMode)
{
    const auto quad = IRenderer::LineToQuad({ x0, y0 }, { x1, y1 });
    const u32 mode = static_cast<u32>(IRenderer::PsxDrawMode::Flat);
    const u32 semi = static_cast<u32>(semiTransparent);
    const u32 blend = static_cast<u32>(blendMode);
    Batch batch;
    batch.scissor = mClipRect;
    batch.blendMode = blendMode;
    batch.vertices = {
        { quad.verts[0].x, quad.verts[0].y, static_cast<f32>(r0), static_cast<f32>(g0), static_cast<f32>(b0), 0, 0, mode, semi, 0, blend, 0, 0 },
        { quad.verts[1].x, quad.verts[1].y, static_cast<f32>(r0), static_cast<f32>(g0), static_cast<f32>(b0), 0, 0, mode, semi, 0, blend, 0, 0 },
        { quad.verts[2].x, quad.verts[2].y, static_cast<f32>(r1), static_cast<f32>(g1), static_cast<f32>(b1), 0, 0, mode, semi, 0, blend, 0, 0 },
        { quad.verts[3].x, quad.verts[3].y, static_cast<f32>(r1), static_cast<f32>(g1), static_cast<f32>(b1), 0, 0, mode, semi, 0, blend, 0, 0 }
    };
    batch.indices = { 0, 1, 2, 1, 2, 3 };
    mBatches.emplace_back(std::move(batch));
}

void SDLGPURenderer::Draw(const Prim_GasEffect& gasEffect)
{
    AddGasQuad(gasEffect);
}

void SDLGPURenderer::AddGasQuad(const Prim_GasEffect& gasEffect)
{
    if (!gasEffect.pGasPixels || !mCommandBuffer)
    {
        return;
    }

    const s32 gasWidth = static_cast<s32>((gasEffect.w - gasEffect.x) / 4);
    const s32 gasHeight = static_cast<s32>((gasEffect.h - gasEffect.y) / 2);
    if (gasWidth <= 0 || gasHeight <= 0 || gasWidth > kPsxFramebufferWidth || gasHeight > kPsxFramebufferHeight)
    {
        return;
    }

    std::vector<RGBA32> pixels(kPsxFramebufferWidth * kPsxFramebufferHeight, RGBA32{ 0, 0, 0, 0 });
    for (s32 y = 0; y < gasHeight; y++)
    {
        for (s32 x = 0; x < gasWidth; x++)
        {
            const u16 pixel = gasEffect.pGasPixels[y * gasWidth + x];
            pixels[y * kPsxFramebufferWidth + x] = {
                static_cast<u8>(((pixel >> 11) & 0x1f) << 3),
                static_cast<u8>(((pixel >> 5) & 0x3f) << 2),
                static_cast<u8>((pixel & 0x1f) << 3),
                255
            };
        }
    }
    mGasTexture->Upload(mCommandBuffer, pixels.data(), kPsxFramebufferWidth * sizeof(RGBA32));

    const u32 mode = static_cast<u32>(IRenderer::PsxDrawMode::Gas);
    const u32 blend = static_cast<u32>(relive::TBlendModes::eBlend_0);
    Batch batch;
    batch.texture = mGasTexture;
    batch.scissor = mClipRect;
    batch.blendMode = relive::TBlendModes::eBlend_0;
    batch.vertices = {
        { static_cast<f32>(gasEffect.x), static_cast<f32>(gasEffect.y), 127, 127, 127, 0, 0, mode, 1, 1, blend, 0, 0 },
        { static_cast<f32>(gasEffect.w), static_cast<f32>(gasEffect.y), 127, 127, 127, static_cast<f32>(gasWidth), 0, mode, 1, 1, blend, 0, 0 },
        { static_cast<f32>(gasEffect.x), static_cast<f32>(gasEffect.h), 127, 127, 127, 0, static_cast<f32>(gasHeight), mode, 1, 1, blend, 0, 0 },
        { static_cast<f32>(gasEffect.w), static_cast<f32>(gasEffect.h), 127, 127, 127, static_cast<f32>(gasWidth), static_cast<f32>(gasHeight), mode, 1, 1, blend, 0, 0 }
    };
    batch.indices = { 0, 1, 2, 1, 2, 3 };
    mBatches.emplace_back(std::move(batch));
}

void SDLGPURenderer::AddFramebufferQuad(SDL_GPUTexture* texture)
{
    Batch batch;
    batch.framebufferTexture = texture;
    batch.scissor = {};
    batch.vertices = {
        { 0, 0, 127, 127, 127, 0, 0, 5, 0, 0, 0, 0, 0 },
        { static_cast<f32>(kPsxFramebufferWidth), 0, 127, 127, 127, static_cast<f32>(kPsxFramebufferWidth), 0, 5, 0, 0, 0, 0, 0 },
        { 0, static_cast<f32>(kPsxFramebufferHeight), 127, 127, 127, 0, static_cast<f32>(kPsxFramebufferHeight), 5, 0, 0, 0, 0, 0 },
        { static_cast<f32>(kPsxFramebufferWidth), static_cast<f32>(kPsxFramebufferHeight), 127, 127, 127, static_cast<f32>(kPsxFramebufferWidth), static_cast<f32>(kPsxFramebufferHeight), 5, 0, 0, 0, 0, 0 }
    };
    batch.indices = { 0, 1, 2, 1, 2, 3 };
    mBatches.emplace_back(std::move(batch));
}

void SDLGPURenderer::Draw(const Line_G2& line)
{
    AddLineSegment(line.X0(), line.Y0(), line.X1(), line.Y1(), line.R0(), line.G0(), line.B0(), line.R0(), line.G0(), line.B0(), line.mSemiTransparent, line.mBlendMode);
}

void SDLGPURenderer::Draw(const Line_G4& line)
{
    AddLineSegment(line.X0(), line.Y0(), line.X1(), line.Y1(), line.R0(), line.G0(), line.B0(), line.R1(), line.G1(), line.B1(), line.mSemiTransparent, line.mBlendMode);
    AddLineSegment(line.X1(), line.Y1(), line.X2(), line.Y2(), line.R1(), line.G1(), line.B1(), line.R2(), line.G2(), line.B2(), line.mSemiTransparent, line.mBlendMode);
    AddLineSegment(line.X2(), line.Y2(), line.X3(), line.Y3(), line.R2(), line.G2(), line.B2(), line.R3(), line.G3(), line.B3(), line.mSemiTransparent, line.mBlendMode);
}

void SDLGPURenderer::Draw(const Poly_G3& poly)
{
    Batch batch;
    batch.scissor = mClipRect;
    batch.blendMode = poly.mBlendMode;
    const u32 mode = static_cast<u32>(IRenderer::PsxDrawMode::Flat);
    const u32 semiTrans = static_cast<u32>(poly.mSemiTransparent);
    const u32 shaded = static_cast<u32>(poly.mIsShaded);
    const u32 blend = static_cast<u32>(poly.mBlendMode);
    batch.vertices = {
        { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), static_cast<f32>(poly.R0()), static_cast<f32>(poly.G0()), static_cast<f32>(poly.B0()), 0, 0, mode, semiTrans, shaded, blend, 0, 0 },
        { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), static_cast<f32>(poly.R1()), static_cast<f32>(poly.G1()), static_cast<f32>(poly.B1()), 0, 0, mode, semiTrans, shaded, blend, 0, 0 },
        { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), static_cast<f32>(poly.R2()), static_cast<f32>(poly.G2()), static_cast<f32>(poly.B2()), 0, 0, mode, semiTrans, shaded, blend, 0, 0 }
    };
    batch.indices = { 0, 1, 2 };
    mBatches.emplace_back(std::move(batch));
}

void SDLGPURenderer::Draw(const Poly_FT4& poly)
{
    if (!poly.mFg1 && !poly.mCam && !poly.mAnim && !poly.mFont)
    {
        AddScreenWaveQuad(poly);
        return;
    }
    const std::shared_ptr<SDLGPUTexture> texture = PrepareTexture(poly);
    if (texture)
    {
        u32 paletteIndex = 0;
        if (poly.mAnim)
        {
            paletteIndex = PreparePalette(*poly.mAnim->mAnimRes.mCurPal);
        }
        else if (poly.mFont)
        {
            paletteIndex = PreparePalette(*poly.mFont->mFntResource.mCurPal);
        }
        AddTexturedQuad(poly, texture, paletteIndex);
    }
    else
    {
        const u32 color = Color(255, 255, 255);
        AddQuad({ static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), 0, 0, color }, { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), 0, 0, color }, { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), 0, 0, color }, { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), 0, 0, color });
    }
}

void SDLGPURenderer::AddScreenWaveQuad(const Poly_FT4& poly)
{
    const f32 uBase = poly.uBase;
    const f32 vBase = poly.vBase;
    const u32 mode = 5;
    const u32 blend = static_cast<u32>(relive::TBlendModes::eBlend_0);
    auto vertex = [&](s16 x, s16 y, u8 u, u8 v)
    {
        return Vertex{ static_cast<f32>(x), static_cast<f32>(y), 127, 127, 127, uBase + u, static_cast<f32>(kPsxFramebufferHeight) - (vBase + v), mode, 0, 0, blend, 0, 0 };
    };

    Batch batch;
    batch.framebufferTexture = mPreviousFramebuffer;
    batch.blendMode = relive::TBlendModes::eBlend_0;
    batch.scissor = mClipRect;
    batch.vertices = { vertex(poly.X0(), poly.Y0(), poly.U0(), poly.V0()), vertex(poly.X1(), poly.Y1(), poly.U1(), poly.V1()), vertex(poly.X2(), poly.Y2(), poly.U2(), poly.V2()), vertex(poly.X3(), poly.Y3(), poly.U3(), poly.V3()) };
    batch.indices = { 0, 1, 2, 1, 2, 3 };
    mBatches.emplace_back(std::move(batch));
}


void SDLGPURenderer::Draw(const Poly_G4& poly)
{
    Batch batch;
    batch.scissor = mClipRect;
    batch.blendMode = poly.mBlendMode;
    const u32 mode = static_cast<u32>(IRenderer::PsxDrawMode::Flat);
    const u32 semiTrans = static_cast<u32>(poly.mSemiTransparent);
    const u32 shaded = static_cast<u32>(poly.mIsShaded);
    const u32 blend = static_cast<u32>(poly.mBlendMode);
    batch.vertices = {
        { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), static_cast<f32>(poly.R0()), static_cast<f32>(poly.G0()), static_cast<f32>(poly.B0()), 0, 0, mode, semiTrans, shaded, blend, 0, 0 },
        { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), static_cast<f32>(poly.R1()), static_cast<f32>(poly.G1()), static_cast<f32>(poly.B1()), 0, 0, mode, semiTrans, shaded, blend, 0, 0 },
        { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), static_cast<f32>(poly.R2()), static_cast<f32>(poly.G2()), static_cast<f32>(poly.B2()), 0, 0, mode, semiTrans, shaded, blend, 0, 0 },
        { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), static_cast<f32>(poly.R3()), static_cast<f32>(poly.G3()), static_cast<f32>(poly.B3()), 0, 0, mode, semiTrans, shaded, blend, 0, 0 }
    };
    batch.indices = { 0, 1, 2, 1, 2, 3 };
    mBatches.emplace_back(std::move(batch));
}

void SDLGPURenderer::SetClip(const Prim_ScissorRect& clipper)
{
    if (clipper.mRect.x == 0 && clipper.mRect.y == 0 && clipper.mRect.w == 1 && clipper.mRect.h == 1)
    {
        mClipRect = {};
        return;
    }

    mClipRect = { clipper.mRect.x, clipper.mRect.y, clipper.mRect.w, clipper.mRect.h };
}

std::shared_ptr<SDLGPUTexture> SDLGPURenderer::PrepareTexture(const Poly_FT4& poly)
{
    constexpr u32 kTextureLifetime = 300;
    u32 width = 0;
    u32 height = 0;
    u32 bytesPerPixel = sizeof(RGBA32);
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    std::vector<u8> pixels;

    if (poly.mCam)
    {
        mCameraTexture = mTextureCache.GetCachedTexture(poly.mCam->mUniqueId.Id(), kTextureLifetime);
        if (!mCameraTexture)
        {
            const u32 cameraWidth = poly.mCam->mData.mWidth;
            const u32 cameraHeight = poly.mCam->mData.mHeight;
            const std::vector<u8>& cameraPixels = *poly.mCam->mData.mPixels;
            if (cameraWidth > 0 && cameraHeight > 0 && cameraPixels.size() == cameraWidth * cameraHeight * sizeof(RGBA32))
            {
                auto camera = std::make_shared<SDLGPUTexture>(mContext.Device(), cameraWidth, cameraHeight, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
                camera->Upload(mCommandBuffer, cameraPixels.data(), cameraWidth * sizeof(RGBA32));
                mCameraTexture = mTextureCache.Add(poly.mCam->mUniqueId.Id(), kTextureLifetime, camera);
            }
        }
    }

    u32 textureId = 0;
    if (poly.mFg1)
    {
        textureId = poly.mFg1->mUniqueId.Id();
        width = poly.mFg1->mImage.mWidth;
        height = poly.mFg1->mImage.mHeight;
        pixels = *poly.mFg1->mImage.mPixels;
    }
    else if (poly.mCam)
    {
        return mCameraTexture;
    }
    else if (poly.mAnim)
    {
        textureId = poly.mAnim->mAnimRes.mUniqueId.Id();
        const AnimResource& resource = poly.mAnim->mAnimRes;
        width = resource.mPngPtr->mWidth;
        height = resource.mPngPtr->mHeight;
        pixels = resource.mPngPtr->mPixels;
        bytesPerPixel = 1;
        format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    }
    else if (poly.mFont)
    {
        textureId = poly.mFont->mFntResource.mUniqueId.Id();
        const FontResource& resource = poly.mFont->mFntResource;
        width = resource.mPngPtr->mWidth;
        height = resource.mPngPtr->mHeight;
        pixels = resource.mPngPtr->mPixels;
        bytesPerPixel = 1;
        format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    }

    if (width == 0 || height == 0 || pixels.size() != width * height * bytesPerPixel)
    {
        return nullptr;
    }

    auto texture = mTextureCache.GetCachedTexture(textureId, kTextureLifetime);
    if (texture)
    {
        return texture;
    }
    texture = std::make_shared<SDLGPUTexture>(mContext.Device(), width, height, format, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    texture->Upload(mCommandBuffer, pixels.data(), width * bytesPerPixel, bytesPerPixel);
    return mTextureCache.Add(textureId, kTextureLifetime, texture);
}

void SDLGPURenderer::AddTexturedQuad(const Poly_FT4& poly, const std::shared_ptr<SDLGPUTexture>& texture, u32 paletteIndex)
{
    Batch batch;
    batch.texture = texture;
    batch.cameraTexture = mCameraTexture;
    batch.scissor = mClipRect;
    batch.blendMode = poly.mBlendMode;

    f32 u0 = static_cast<f32>(poly.U0());
    f32 v0 = static_cast<f32>(poly.V0());
    f32 u1 = static_cast<f32>(poly.U3());
    f32 v1 = static_cast<f32>(poly.V3());

    if (poly.mCam || poly.mFg1)
    {
        u0 = 0.0f;
        v0 = 0.0f;
        u1 = static_cast<f32>(kPsxFramebufferWidth);
        v1 = static_cast<f32>(kPsxFramebufferHeight);
    }
    else if (poly.mAnim)
    {
        const PerFrameInfo* frame = poly.mAnim->Get_FrameHeader(-1);
        u0 = static_cast<f32>(frame->mSpriteSheetX);
        v0 = static_cast<f32>(frame->mSpriteSheetY);
        u1 = static_cast<f32>(frame->mSpriteSheetX + frame->mSpriteWidth - 1);
        v1 = static_cast<f32>(frame->mSpriteSheetY + frame->mSpriteHeight - 1);

        if (poly.mFlipX)
        {
            std::swap(u0, u1);
        }
        if (poly.mFlipY)
        {
            std::swap(v0, v1);
        }
    }

    batch.vertices = {
        { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), static_cast<f32>(poly.R0()), static_cast<f32>(poly.G0()), static_cast<f32>(poly.B0()), u0, v0, static_cast<u32>(poly.mFg1 ? IRenderer::PsxDrawMode::FG1 : (poly.mCam ? IRenderer::PsxDrawMode::Camera : IRenderer::PsxDrawMode::DefaultFT4)), static_cast<u32>(poly.mSemiTransparent), static_cast<u32>(poly.mIsShaded), static_cast<u32>(poly.mBlendMode), paletteIndex, 0 },
        { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), static_cast<f32>(poly.R0()), static_cast<f32>(poly.G0()), static_cast<f32>(poly.B0()), u1, v0, static_cast<u32>(poly.mFg1 ? IRenderer::PsxDrawMode::FG1 : (poly.mCam ? IRenderer::PsxDrawMode::Camera : IRenderer::PsxDrawMode::DefaultFT4)), static_cast<u32>(poly.mSemiTransparent), static_cast<u32>(poly.mIsShaded), static_cast<u32>(poly.mBlendMode), paletteIndex, 0 },
        { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), static_cast<f32>(poly.R0()), static_cast<f32>(poly.G0()), static_cast<f32>(poly.B0()), u0, v1, static_cast<u32>(poly.mFg1 ? IRenderer::PsxDrawMode::FG1 : (poly.mCam ? IRenderer::PsxDrawMode::Camera : IRenderer::PsxDrawMode::DefaultFT4)), static_cast<u32>(poly.mSemiTransparent), static_cast<u32>(poly.mIsShaded), static_cast<u32>(poly.mBlendMode), paletteIndex, 0 },
        { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), static_cast<f32>(poly.R0()), static_cast<f32>(poly.G0()), static_cast<f32>(poly.B0()), u1, v1, static_cast<u32>(poly.mFg1 ? IRenderer::PsxDrawMode::FG1 : (poly.mCam ? IRenderer::PsxDrawMode::Camera : IRenderer::PsxDrawMode::DefaultFT4)), static_cast<u32>(poly.mSemiTransparent), static_cast<u32>(poly.mIsShaded), static_cast<u32>(poly.mBlendMode), paletteIndex, 0 }
    };
    batch.indices = { 0, 1, 2, 1, 2, 3 };
    mBatches.emplace_back(std::move(batch));
}

u32 SDLGPURenderer::PreparePalette(const AnimationPal& palette)
{
    u32 hash = 5381;
    for (const RGBA32& color : palette.mPal)
    {
        hash = ((hash << 5) + hash) + color.r;
        hash = ((hash << 5) + hash) + color.g;
        hash = ((hash << 5) + hash) + color.b;
        hash = ((hash << 5) + hash) + color.a;
    }

    const auto existing = mPaletteRows.find(hash);
    if (existing != mPaletteRows.end())
    {
        return existing->second;
    }

    const u32 row = static_cast<u32>(mPaletteRows.size());
    if (row >= 256)
    {
        return 0;
    }
    mPaletteRows.emplace(hash, row);
    std::memcpy(mPalettePixels.data() + row * 256, palette.mPal, 256 * sizeof(RGBA32));
    return row;
}

void SDLGPURenderer::UploadPalettes()
{
    if (mPaletteRows.empty() || !mCommandBuffer)
    {
        return;
    }

    void* pixels = SDL_MapGPUTransferBuffer(mContext.Device(), mTextureTransferBuffer, true);
    std::memcpy(pixels, mPalettePixels.data(), mPalettePixels.size() * sizeof(RGBA32));
    SDL_UnmapGPUTransferBuffer(mContext.Device(), mTextureTransferBuffer);

    SDL_GPUTextureTransferInfo source = {};
    source.transfer_buffer = mTextureTransferBuffer;
    source.pixels_per_row = 256;
    source.rows_per_layer = 256;
    SDL_GPUTextureRegion destination = {};
    destination.texture = mPaletteTexture;
    destination.w = 256;
    destination.h = 256;
    destination.d = 1;

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(mCommandBuffer);
    SDL_UploadToGPUTexture(copyPass, &source, &destination, true);
    SDL_EndGPUCopyPass(copyPass);
}

void SDLGPURenderer::UploadWhiteTexture()
{
    if (mWhiteTextureUploaded || !mCommandBuffer)
    {
        return;
    }

    Uint8* pixels = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(mContext.Device(), mTextureTransferBuffer, true));
    pixels[0] = 255;
    pixels[1] = 255;
    pixels[2] = 255;
    pixels[3] = 255;
    SDL_UnmapGPUTransferBuffer(mContext.Device(), mTextureTransferBuffer);

    SDL_GPUTextureTransferInfo source = {};
    source.transfer_buffer = mTextureTransferBuffer;
    source.pixels_per_row = 1;
    source.rows_per_layer = 1;
    SDL_GPUTextureRegion destination = {};
    destination.texture = mWhiteTexture;
    destination.w = 1;
    destination.h = 1;
    destination.d = 1;

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(mCommandBuffer);
    SDL_UploadToGPUTexture(copyPass, &source, &destination, true);
    SDL_EndGPUCopyPass(copyPass);
    mWhiteTextureUploaded = true;
}

void SDLGPURenderer::DrawVertices(SDL_GPURenderPass* renderPass, Batch& batch)
{
    if (batch.vertices.empty() || batch.indices.empty())
    {
        return;
    }

    SDL_BindGPUGraphicsPipeline(renderPass, batch.blendMode == relive::TBlendModes::eBlend_2 ? mSubtractPipeline : mPipeline);
    SDL_GPUBufferBinding vertexBinding = { batch.vertexBuffer, 0 };
    SDL_GPUBufferBinding indexBinding = { batch.indexBuffer, 0 };
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_GPUSampler* sampler = batch.presentation && mFramebufferFilter ? mLinearSampler : mSampler;
    SDL_GPUTextureSamplerBinding textureBindings[5] = {};
    textureBindings[0] = { mPaletteTexture, sampler };
    textureBindings[1] = { batch.cameraTexture ? batch.cameraTexture->Get() : mWhiteTexture, sampler };
    textureBindings[2] = { batch.texture ? batch.texture->Get() : mWhiteTexture, sampler };
    textureBindings[3] = { mGasTexture->Get(), sampler };
    textureBindings[4] = { batch.framebufferTexture ? batch.framebufferTexture : mPreviousFramebuffer, sampler };
    SDL_BindGPUFragmentSamplers(renderPass, 0, textureBindings, 5);
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSizeInPixels(mWindow, &windowWidth, &windowHeight);
    const float transform[] = {
        batch.presentation ? static_cast<float>(windowWidth) : static_cast<float>(kPsxFramebufferWidth),
        batch.presentation ? static_cast<float>(windowHeight) : static_cast<float>(kPsxFramebufferHeight),
        0.0f,
        0.0f
    };
    SDL_PushGPUVertexUniformData(mCommandBuffer, 0, transform, sizeof(transform));
    struct FragmentUniforms final
    {
        float spriteSheetSize[12][2] = {};
        u32 drawingFramebuffer = 0;
    } fragmentUniforms;
    SDL_PushGPUFragmentUniformData(mCommandBuffer, 0, &fragmentUniforms, sizeof(fragmentUniforms));
    if (batch.scissor.w > 0 && batch.scissor.h > 0)
    {
        SDL_SetGPUScissor(renderPass, &batch.scissor);
    }
    else
    {
        SDL_SetGPUScissor(renderPass, nullptr);
    }
    SDL_DrawGPUIndexedPrimitives(renderPass, static_cast<Uint32>(batch.indices.size()), 1, 0, 0, 0);
}

void SDLGPURenderer::UploadBatch(Batch& batch)
{
    if (batch.vertices.empty() || batch.indices.empty())
    {
        return;
    }

    batch.vertexBuffer = CreateBuffer(mContext.Device(), SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(batch.vertices.size() * sizeof(Vertex)));
    batch.indexBuffer = CreateBuffer(mContext.Device(), SDL_GPU_BUFFERUSAGE_INDEX, static_cast<Uint32>(batch.indices.size() * sizeof(u16)));

    void* vertexData = SDL_MapGPUTransferBuffer(mContext.Device(), mVertexTransferBuffer, true);
    std::memcpy(vertexData, batch.vertices.data(), batch.vertices.size() * sizeof(Vertex));
    SDL_UnmapGPUTransferBuffer(mContext.Device(), mVertexTransferBuffer);
    void* indexData = SDL_MapGPUTransferBuffer(mContext.Device(), mIndexTransferBuffer, true);
    std::memcpy(indexData, batch.indices.data(), batch.indices.size() * sizeof(u16));
    SDL_UnmapGPUTransferBuffer(mContext.Device(), mIndexTransferBuffer);

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(mCommandBuffer);
    SDL_GPUTransferBufferLocation vertexSource = { mVertexTransferBuffer, 0 };
    SDL_GPUBufferRegion vertexDestination = { batch.vertexBuffer, 0, static_cast<Uint32>(batch.vertices.size() * sizeof(Vertex)) };
    SDL_GPUTransferBufferLocation indexSource = { mIndexTransferBuffer, 0 };
    SDL_GPUBufferRegion indexDestination = { batch.indexBuffer, 0, static_cast<Uint32>(batch.indices.size() * sizeof(u16)) };
    SDL_UploadToGPUBuffer(copyPass, &vertexSource, &vertexDestination, true);
    SDL_UploadToGPUBuffer(copyPass, &indexSource, &indexDestination, true);
    SDL_EndGPUCopyPass(copyPass);
}

void SDLGPURenderer::DumpFrameIfDue()
{
    if (mFrameDumped || SDL_GetTicks() - mRendererStartTicks < 4000 || !mSwapchainTexture || !mCommandBuffer)
    {
        return;
    }

    int width = 0;
    int height = 0;
    size_t queuedVertices = 0;
    size_t queuedIndices = 0;
    f32 minVertexX = 100000.0f;
    f32 minVertexY = 100000.0f;
    f32 maxVertexX = -100000.0f;
    f32 maxVertexY = -100000.0f;
    for (const Batch& batch : mBatches)
    {
        queuedVertices += batch.vertices.size();
        queuedIndices += batch.indices.size();
        for (const Vertex& vertex : batch.vertices)
        {
            minVertexX = vertex.x < minVertexX ? vertex.x : minVertexX;
            minVertexY = vertex.y < minVertexY ? vertex.y : minVertexY;
            maxVertexX = vertex.x > maxVertexX ? vertex.x : maxVertexX;
            maxVertexY = vertex.y > maxVertexY ? vertex.y : maxVertexY;
        }
    }
    SDL_GetWindowSizeInPixels(mWindow, &width, &height);

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transferInfo.size = static_cast<Uint32>(width * height * 4);
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(mContext.Device(), &transferInfo);
    if (!transferBuffer)
    {
        LOG_ERROR("SDL GPU frame dump buffer creation failed: %s", SDL_GetError());
        mFrameDumped = true;
        return;
    }

    SDL_GPUTextureRegion source = {};
    source.texture = mSwapchainTexture;
    source.w = static_cast<Uint32>(width);
    source.h = static_cast<Uint32>(height);
    source.d = 1;
    SDL_GPUTextureTransferInfo destination = {};
    destination.transfer_buffer = transferBuffer;
    destination.pixels_per_row = static_cast<Uint32>(width);
    destination.rows_per_layer = static_cast<Uint32>(height);

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(mCommandBuffer);
    SDL_DownloadFromGPUTexture(copyPass, &source, &destination);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(mCommandBuffer);
    SDL_WaitForGPUIdle(mContext.Device());

    const Uint8* pixels = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(mContext.Device(), transferBuffer, false));
    if (pixels)
    {
        Uint8 minValue = 255;
        Uint8 maxValue = 0;
        bool allSame = true;
        size_t nonBlackPixels = 0;
        int minX = width;
        int minY = height;
        int maxX = -1;
        int maxY = -1;
        const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
        for (size_t i = 0; i < byteCount; i++)
        {
            minValue = pixels[i] < minValue ? pixels[i] : minValue;
            maxValue = pixels[i] > maxValue ? pixels[i] : maxValue;
            if (i > 0 && pixels[i] != pixels[0])
            {
                allSame = false;
            }

            if ((i % 4) == 0 && (pixels[i] != 0 || pixels[i + 1] != 0 || pixels[i + 2] != 0))
            {
                const int pixelIndex = static_cast<int>(i / 4);
                const int x = pixelIndex % width;
                const int y = pixelIndex / width;
                nonBlackPixels++;
                minX = x < minX ? x : minX;
                minY = y < minY ? y : minY;
                maxX = x > maxX ? x : maxX;
                maxY = y > maxY ? y : maxY;
            }
        }

        FILE* file = std::fopen("/tmp/relive-sdlgpu-frame.ppm", "wb");
        if (file)
        {
            std::fprintf(file, "P6\n%d %d\n255\n", width, height);
            for (size_t i = 0; i < byteCount; i += 4)
            {
                std::fputc(pixels[i + 0], file);
                std::fputc(pixels[i + 1], file);
                std::fputc(pixels[i + 2], file);
            }
            std::fclose(file);
        }
        LOG_INFO("SDL GPU frame dump: vertices=%zu indices=%zu vertex_bounds=%.1f,%.1f-%.1f,%.1f min=%u max=%u all_same=%s non_black=%zu bounds=%d,%d-%d,%d first=%u,%u,%u,%u", queuedVertices, queuedIndices, minVertexX, minVertexY, maxVertexX, maxVertexY, minValue, maxValue, allSame ? "true" : "false", nonBlackPixels, minX, minY, maxX, maxY, pixels[0], pixels[1], pixels[2], pixels[3]);
        SDL_UnmapGPUTransferBuffer(mContext.Device(), transferBuffer);
    }
    else
    {
        LOG_ERROR("SDL GPU frame dump mapping failed: %s", SDL_GetError());
    }
    SDL_ReleaseGPUTransferBuffer(mContext.Device(), transferBuffer);
    mFrameDumped = true;
    mCommandBuffer = nullptr;
    mSwapchainTexture = nullptr;
}

void SDLGPURenderer::EndFrame()
{
    Batch presentation;
    if (mCommandBuffer && mSwapchainTexture)
    {
        UploadWhiteTexture();
        UploadPalettes();
        for (Batch& batch : mBatches)
        {
            if (!batch.vertices.empty() && batch.vertices.front().drawMode == static_cast<u32>(IRenderer::PsxDrawMode::FG1))
            {
                batch.cameraTexture = mCameraTexture;
            }
        }
        for (Batch& batch : mBatches)
        {
            UploadBatch(batch);
        }
        SDL_GPUColorTargetInfo targetInfo = {};
        targetInfo.texture = mCurrentFramebuffer;
        targetInfo.clear_color = { static_cast<float>(mClearR) / 255.0f, static_cast<float>(mClearG) / 255.0f, static_cast<float>(mClearB) / 255.0f, 1.0f };
        targetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        targetInfo.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(mCommandBuffer, &targetInfo, 1, nullptr);
        for (Batch& batch : mBatches)
        {
            DrawVertices(renderPass, batch);
        }
        SDL_EndGPURenderPass(renderPass);

        presentation.framebufferTexture = mCurrentFramebuffer;
        presentation.presentation = true;
        const SDL_Rect drawRect = GetTargetDrawRect();
        presentation.vertices = {
            { static_cast<f32>(drawRect.x), static_cast<f32>(drawRect.y), 127, 127, 127, 0, 0, 5, 0, 0, 0, 0, 0 },
            { static_cast<f32>(drawRect.x + drawRect.w), static_cast<f32>(drawRect.y), 127, 127, 127, static_cast<f32>(kPsxFramebufferWidth), 0, 5, 0, 0, 0, 0, 0 },
            { static_cast<f32>(drawRect.x), static_cast<f32>(drawRect.y + drawRect.h), 127, 127, 127, 0, static_cast<f32>(kPsxFramebufferHeight), 5, 0, 0, 0, 0, 0 },
            { static_cast<f32>(drawRect.x + drawRect.w), static_cast<f32>(drawRect.y + drawRect.h), 127, 127, 127, static_cast<f32>(kPsxFramebufferWidth), static_cast<f32>(kPsxFramebufferHeight), 5, 0, 0, 0, 0, 0 }
        };
        presentation.indices = { 0, 1, 2, 1, 2, 3 };
        UploadBatch(presentation);
        SDL_GPUColorTargetInfo swapchainTarget = {};
        swapchainTarget.texture = mSwapchainTexture;
        swapchainTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        swapchainTarget.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* presentationPass = SDL_BeginGPURenderPass(mCommandBuffer, &swapchainTarget, 1, nullptr);
        DrawVertices(presentationPass, presentation);
        SDL_EndGPURenderPass(presentationPass);
        DumpFrameIfDue();
        mHasPreviousFramebuffer = true;
        std::swap(mCurrentFramebuffer, mPreviousFramebuffer);
    }
    if (mCommandBuffer)
    {
        mContext.EndFrame(mCommandBuffer);
        mCommandBuffer = nullptr;
        mSwapchainTexture = nullptr;
    }

    SDL_WaitForGPUIdle(mContext.Device());
    mTextureCache.DecreaseResourceLifetimes();
    for (Batch& batch : mBatches)
    {
        SDL_ReleaseGPUBuffer(mContext.Device(), batch.vertexBuffer);
        SDL_ReleaseGPUBuffer(mContext.Device(), batch.indexBuffer);
        batch.vertexBuffer = nullptr;
        batch.indexBuffer = nullptr;
    }
    SDL_ReleaseGPUBuffer(mContext.Device(), presentation.vertexBuffer);
    SDL_ReleaseGPUBuffer(mContext.Device(), presentation.indexBuffer);
    mBatches.clear();
    mBatches.emplace_back();
}

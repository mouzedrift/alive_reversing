#include "SDLGPURenderer.hpp"

#include "../../../relive_lib/Primitives.hpp"
#include "../../../relive_lib/Animation.hpp"
#include "../../../relive_lib/ResourceManagerWrapper.hpp"
#include "../../../relive_lib/data_conversion/rgb_conversion.hpp"
#include "../../../relive_lib/data_conversion/AnimationConverter.hpp"
#include "../../../AliveLibAE/Font.hpp"

#include "generated/colored_vert_spv.hpp"
#include "generated/colored_frag_spv.hpp"

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
    mTextureTransferBuffer = CreateTransferBuffer(mContext.Device(), 4);
}

void SDLGPURenderer::CreatePipeline()
{
    SDL_GPUShaderCreateInfo vertexInfo = {};
    vertexInfo.code_size = sizeof(g_colored_vert_spv);
    vertexInfo.code = g_colored_vert_spv;
    vertexInfo.entrypoint = "main";
    vertexInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vertexInfo.num_uniform_buffers = 1;
    mVertexShader = SDL_CreateGPUShader(mContext.Device(), &vertexInfo);

    SDL_GPUShaderCreateInfo fragmentInfo = {};
    fragmentInfo.code_size = sizeof(g_colored_frag_spv);
    fragmentInfo.code = g_colored_frag_spv;
    fragmentInfo.entrypoint = "main";
    fragmentInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fragmentInfo.num_samplers = 1;
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

    SDL_GPUVertexBufferDescription bufferDescription = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
    SDL_GPUVertexAttribute attributes[] = {
        { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, x) },
        { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, u) },
        { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(Vertex, color) }
    };
    SDL_GPUVertexInputState vertexInput = {};
    vertexInput.vertex_buffer_descriptions = &bufferDescription;
    vertexInput.num_vertex_buffers = 1;
    vertexInput.vertex_attributes = attributes;
    vertexInput.num_vertex_attributes = 3;

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
    SDL_ReleaseGPUSampler(mContext.Device(), mSampler);
    SDL_ReleaseGPUGraphicsPipeline(mContext.Device(), mPipeline);
    SDL_ReleaseGPUShader(mContext.Device(), mVertexShader);
    SDL_ReleaseGPUShader(mContext.Device(), mFragmentShader);
}

void SDLGPURenderer::StartFrame()
{
    IRenderer::StartFrame();
    mCommandBuffer = mContext.BeginFrame(&mSwapchainTexture);
}

void SDLGPURenderer::Clear(u8 r, u8 g, u8 b)
{
    mClearR = r;
    mClearG = g;
    mClearB = b;
}

void SDLGPURenderer::AddTriangle(const Vertex& a, const Vertex& b, const Vertex& c)
{
    if (mBatches.back().texture)
    {
        mBatches.emplace_back();
    }
    Batch& batch = mBatches.back();
    const u16 base = static_cast<u16>(batch.vertices.size());
    batch.vertices.insert(batch.vertices.end(), { a, b, c });
    batch.indices.insert(batch.indices.end(), { base, static_cast<u16>(base + 1), static_cast<u16>(base + 2) });
}

void SDLGPURenderer::AddQuad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d)
{
    if (mBatches.back().texture)
    {
        mBatches.emplace_back();
    }
    Batch& batch = mBatches.back();
    const u16 base = static_cast<u16>(batch.vertices.size());
    batch.vertices.insert(batch.vertices.end(), { a, b, c, d });
    batch.indices.insert(batch.indices.end(), { base, static_cast<u16>(base + 1), static_cast<u16>(base + 2), static_cast<u16>(base + 1), static_cast<u16>(base + 2), static_cast<u16>(base + 3) });
}

void SDLGPURenderer::Draw(const Prim_GasEffect& gasEffect)
{
    const u32 color = Color(128, 128, 128);
    AddQuad({ static_cast<f32>(gasEffect.x), static_cast<f32>(gasEffect.y), 0, 0, color }, { static_cast<f32>(gasEffect.w), static_cast<f32>(gasEffect.y), 0, 0, color }, { static_cast<f32>(gasEffect.x), static_cast<f32>(gasEffect.h), 0, 0, color }, { static_cast<f32>(gasEffect.w), static_cast<f32>(gasEffect.h), 0, 0, color });
}

void SDLGPURenderer::Draw(const Line_G2& line)
{
    const auto quad = IRenderer::LineToQuad({ line.X0(), line.Y0() }, { line.X1(), line.Y1() });
    const u32 color = Color(line.R0(), line.G0(), line.B0());
    AddQuad({ quad.verts[0].x, quad.verts[0].y, 0, 0, color }, { quad.verts[1].x, quad.verts[1].y, 0, 0, color }, { quad.verts[2].x, quad.verts[2].y, 0, 0, color }, { quad.verts[3].x, quad.verts[3].y, 0, 0, color });
}

void SDLGPURenderer::Draw(const Line_G4& line)
{
    const auto addLine = [this](f32 x0, f32 y0, f32 x1, f32 y1, u32 color)
    {
        const auto quad = IRenderer::LineToQuad({ x0, y0 }, { x1, y1 });
        AddQuad({ quad.verts[0].x, quad.verts[0].y, 0, 0, color }, { quad.verts[1].x, quad.verts[1].y, 0, 0, color }, { quad.verts[2].x, quad.verts[2].y, 0, 0, color }, { quad.verts[3].x, quad.verts[3].y, 0, 0, color });
    };
    addLine(line.X0(), line.Y0(), line.X1(), line.Y1(), Color(line.R0(), line.G0(), line.B0()));
    addLine(line.X1(), line.Y1(), line.X2(), line.Y2(), Color(line.R1(), line.G1(), line.B1()));
    addLine(line.X2(), line.Y2(), line.X3(), line.Y3(), Color(line.R2(), line.G2(), line.B2()));
}

void SDLGPURenderer::Draw(const Poly_G3& poly)
{
    AddTriangle({ static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), 0, 0, Color(poly.R0(), poly.G0(), poly.B0()) }, { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), 0, 0, Color(poly.R1(), poly.G1(), poly.B1()) }, { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), 0, 0, Color(poly.R2(), poly.G2(), poly.B2()) });
}

void SDLGPURenderer::Draw(const Poly_FT4& poly)
{
    const std::shared_ptr<SDLGPUTexture> texture = PrepareTexture(poly);
    if (texture)
    {
        AddTexturedQuad(poly, texture);
    }
    else
    {
        const u32 color = Color(255, 255, 255);
        AddQuad({ static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), 0, 0, color }, { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), 0, 0, color }, { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), 0, 0, color }, { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), 0, 0, color });
    }
}

void SDLGPURenderer::Draw(const Poly_G4& poly)
{
    AddQuad({ static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), 0, 0, Color(poly.R0(), poly.G0(), poly.B0()) }, { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), 0, 0, Color(poly.R1(), poly.G1(), poly.B1()) }, { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), 0, 0, Color(poly.R2(), poly.G2(), poly.B2()) }, { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), 0, 0, Color(poly.R3(), poly.G3(), poly.B3()) });
}

void SDLGPURenderer::SetClip(const Prim_ScissorRect& clipper)
{
    if (clipper.mRect.x == 0 && clipper.mRect.y == 0 && clipper.mRect.w == 1 && clipper.mRect.h == 1)
    {
        mClipRect = {};
        return;
    }

    mClipRect = { clipper.mRect.x, clipper.mRect.y * 2, clipper.mRect.w, clipper.mRect.h * 2 };
}

std::shared_ptr<SDLGPUTexture> SDLGPURenderer::PrepareTexture(const Poly_FT4& poly)
{
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> pixels;

    if (poly.mFg1)
    {
        width = poly.mFg1->mImage.mWidth;
        height = poly.mFg1->mImage.mHeight;
        pixels = *poly.mFg1->mImage.mPixels;
    }
    else if (poly.mCam)
    {
        width = poly.mCam->mData.mWidth;
        height = poly.mCam->mData.mHeight;
        pixels = *poly.mCam->mData.mPixels;
    }
    else if (poly.mAnim)
    {
        const AnimResource& resource = poly.mAnim->mAnimRes;
        width = resource.mPngPtr->mWidth;
        height = resource.mPngPtr->mHeight;
        pixels.resize(width * height * sizeof(RGBA32));
        for (u32 i = 0; i < width * height; i++)
        {
            const RGBA32 color = resource.mCurPal->mPal[resource.mPngPtr->mPixels[i]];
            std::memcpy(pixels.data() + i * sizeof(RGBA32), &color, sizeof(RGBA32));
        }
    }
    else if (poly.mFont)
    {
        const FontResource& resource = poly.mFont->mFntResource;
        width = resource.mPngPtr->mWidth;
        height = resource.mPngPtr->mHeight;
        pixels.resize(width * height * sizeof(RGBA32));
        for (u32 i = 0; i < width * height; i++)
        {
            const RGBA32 color = resource.mCurPal->mPal[resource.mPngPtr->mPixels[i]];
            std::memcpy(pixels.data() + i * sizeof(RGBA32), &color, sizeof(RGBA32));
        }
    }

    if (width == 0 || height == 0 || pixels.size() != width * height * sizeof(RGBA32))
    {
        return nullptr;
    }

    auto texture = std::make_shared<SDLGPUTexture>(mContext.Device(), width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    texture->Upload(mCommandBuffer, pixels.data(), width * sizeof(RGBA32));
    return texture;
}

void SDLGPURenderer::AddTexturedQuad(const Poly_FT4& poly, const std::shared_ptr<SDLGPUTexture>& texture)
{
    Batch batch;
    batch.texture = texture;
    const f32 width = static_cast<f32>(texture->Width());
    const f32 height = static_cast<f32>(texture->Height());
    const u32 color = Color(255, 255, 255);
    batch.vertices = {
        { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()), static_cast<f32>(poly.U0()) / width, static_cast<f32>(poly.V0()) / height, color },
        { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()), static_cast<f32>(poly.U1()) / width, static_cast<f32>(poly.V1()) / height, color },
        { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()), static_cast<f32>(poly.U2()) / width, static_cast<f32>(poly.V2()) / height, color },
        { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()), static_cast<f32>(poly.U3()) / width, static_cast<f32>(poly.V3()) / height, color }
    };
    batch.indices = { 0, 1, 2, 1, 2, 3 };
    mBatches.emplace_back(std::move(batch));
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

    SDL_BindGPUGraphicsPipeline(renderPass, mPipeline);
    SDL_GPUBufferBinding vertexBinding = { batch.vertexBuffer, 0 };
    SDL_GPUBufferBinding indexBinding = { batch.indexBuffer, 0 };
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_GPUTextureSamplerBinding textureBinding = { batch.texture ? batch.texture->Get() : mWhiteTexture, mSampler };
    SDL_BindGPUFragmentSamplers(renderPass, 0, &textureBinding, 1);
    const float transform[] = {
        static_cast<float>(kPsxFramebufferWidth),
        static_cast<float>(kPsxFramebufferHeight),
        0.0f,
        0.0f
    };
    SDL_PushGPUVertexUniformData(mCommandBuffer, 0, transform, sizeof(transform));
    if (mClipRect.w > 0 && mClipRect.h > 0)
    {
        SDL_SetGPUScissor(renderPass, &mClipRect);
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
    if (mCommandBuffer && mSwapchainTexture)
    {
        UploadWhiteTexture();
        for (Batch& batch : mBatches)
        {
            UploadBatch(batch);
        }
        SDL_GPUColorTargetInfo targetInfo = {};
        targetInfo.texture = mSwapchainTexture;
        targetInfo.clear_color = { static_cast<float>(mClearR) / 255.0f, static_cast<float>(mClearG) / 255.0f, static_cast<float>(mClearB) / 255.0f, 1.0f };
        targetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        targetInfo.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(mCommandBuffer, &targetInfo, 1, nullptr);
        for (Batch& batch : mBatches)
        {
            DrawVertices(renderPass, batch);
        }
        SDL_EndGPURenderPass(renderPass);
        DumpFrameIfDue();
    }
    if (mCommandBuffer)
    {
        mContext.EndFrame(mCommandBuffer);
        mCommandBuffer = nullptr;
        mSwapchainTexture = nullptr;
    }
    mBatches.clear();
    mBatches.emplace_back();
}

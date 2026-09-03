#pragma once

#include "../IRenderer.hpp"
#include "SDLGPUContext.hpp"
#include "SDLGPUTexture.hpp"
#include "../TextureCache.hpp"
#include "../../data_conversion/AnimationConverter.hpp"
#include "../../data_conversion/rgb_conversion.hpp"

#include <array>
#include <map>
#include <vector>
#include <memory>

class SDLGPURenderer final : public IRenderer
{
public:
    explicit SDLGPURenderer(TWindowHandleType window);
    ~SDLGPURenderer() override;

    void Clear(u8 r, u8 g, u8 b) override;
    void Draw(const Prim_GasEffect& gasEffect) override;
    void Draw(const Line_G2& line) override;
    void Draw(const Line_G4& line) override;
    void Draw(const Poly_G3& poly) override;
    void Draw(const Poly_FT4& poly) override;
    void Draw(const Poly_G4& poly) override;
    void EndFrame() override;
    void SetClip(const Prim_ScissorRect& clipper) override;
    void StartFrame() override;

private:
    struct Vertex final
    {
        Vertex(f32 xIn, f32 yIn, f32 uIn, f32 vIn, u32 packedColor)
            : x(xIn), y(yIn), r(static_cast<f32>(packedColor & 0xff)), g(static_cast<f32>((packedColor >> 8) & 0xff)), b(static_cast<f32>((packedColor >> 16) & 0xff)), u(uIn), v(vIn), drawMode(0), isSemiTrans(0), isShaded(0), blendMode(0), paletteIndex(0), textureUnitIndex(0)
        {
        }

        Vertex(f32 xIn, f32 yIn, f32 rIn, f32 gIn, f32 bIn, f32 uIn, f32 vIn, u32 drawModeIn, u32 isSemiTransIn, u32 isShadedIn, u32 blendModeIn, u32 paletteIndexIn, u32 textureUnitIndexIn)
            : x(xIn), y(yIn), r(rIn), g(gIn), b(bIn), u(uIn), v(vIn), drawMode(drawModeIn), isSemiTrans(isSemiTransIn), isShaded(isShadedIn), blendMode(blendModeIn), paletteIndex(paletteIndexIn), textureUnitIndex(textureUnitIndexIn)
        {
        }

        f32 x, y;
        f32 r, g, b;
        f32 u, v;
        u32 drawMode;
        u32 isSemiTrans;
        u32 isShaded;
        u32 blendMode;
        u32 paletteIndex;
        u32 textureUnitIndex;
    };

    struct Batch final
    {
        std::shared_ptr<SDLGPUTexture> texture;
        std::shared_ptr<SDLGPUTexture> cameraTexture;
        SDL_GPUTexture* framebufferTexture = nullptr;
        SDL_GPUBuffer* vertexBuffer = nullptr;
        SDL_GPUBuffer* indexBuffer = nullptr;
        std::vector<Vertex> vertices;
        std::vector<u16> indices;
        SDL_Rect scissor = {};
        relive::TBlendModes blendMode = relive::TBlendModes::eBlend_0;
        bool presentation = false;
    };

    void AddTriangle(const Vertex& a, const Vertex& b, const Vertex& c);
    void AddQuad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d);
    void AddLineSegment(f32 x0, f32 y0, f32 x1, f32 y1, u8 r0, u8 g0, u8 b0, u8 r1, u8 g1, u8 b1, bool semiTransparent, relive::TBlendModes blendMode);
    void AddGasQuad(const Prim_GasEffect& gasEffect);
    void AddScreenWaveQuad(const Poly_FT4& poly);
    void AddFramebufferQuad(SDL_GPUTexture* texture);
    void AddTexturedQuad(const Poly_FT4& poly, const std::shared_ptr<SDLGPUTexture>& texture, u32 paletteIndex);
    void UploadBatch(Batch& batch);
    void DrawVertices(SDL_GPURenderPass* renderPass, Batch& batch);
    std::shared_ptr<SDLGPUTexture> PrepareTexture(const Poly_FT4& poly);
    void UploadWhiteTexture();
    void CreatePipeline();
    u32 PreparePalette(const AnimationPal& palette);
    void UploadPalettes();
    void DumpFrameIfDue();

    SDLGPUContext mContext;
    SDL_GPUTexture* mSwapchainTexture = nullptr;
    SDL_GPUCommandBuffer* mCommandBuffer = nullptr;
    SDL_GPUShader* mVertexShader = nullptr;
    SDL_GPUShader* mFragmentShader = nullptr;
    SDL_GPUGraphicsPipeline* mPipeline = nullptr;
    SDL_GPUGraphicsPipeline* mSubtractPipeline = nullptr;
    SDL_GPUSampler* mSampler = nullptr;
    SDL_GPUSampler* mLinearSampler = nullptr;
    SDL_GPUTexture* mWhiteTexture = nullptr;
    SDL_GPUTexture* mPaletteTexture = nullptr;
    SDL_GPUTexture* mCurrentFramebuffer = nullptr;
    SDL_GPUTexture* mPreviousFramebuffer = nullptr;
    std::shared_ptr<SDLGPUTexture> mCameraTexture;
    std::shared_ptr<SDLGPUTexture> mGasTexture;
    SDL_GPUTransferBuffer* mVertexTransferBuffer = nullptr;
    SDL_GPUTransferBuffer* mIndexTransferBuffer = nullptr;
    SDL_GPUTransferBuffer* mTextureTransferBuffer = nullptr;
    std::vector<Batch> mBatches;
    TextureCache<std::shared_ptr<SDLGPUTexture>> mTextureCache;
    std::vector<u8> mTexturePixels;
    SDL_Rect mClipRect = {};
    bool mWhiteTextureUploaded = false;
    bool mFrameDumped = false;
    bool mHasPreviousFramebuffer = false;
    std::map<u32, u32> mPaletteRows;
    std::array<RGBA32, 256 * 256> mPalettePixels = {};
    Uint64 mRendererStartTicks = 0;
    u8 mClearR = 0;
    u8 mClearG = 0;
    u8 mClearB = 0;
};

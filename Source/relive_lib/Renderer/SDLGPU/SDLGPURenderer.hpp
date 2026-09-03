#pragma once

#include "../IRenderer.hpp"
#include "SDLGPUContext.hpp"
#include "SDLGPUTexture.hpp"

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
        f32 x, y;
        f32 u, v;
        u32 color;
    };

    struct Batch final
    {
        std::shared_ptr<SDLGPUTexture> texture;
        SDL_GPUBuffer* vertexBuffer = nullptr;
        SDL_GPUBuffer* indexBuffer = nullptr;
        std::vector<Vertex> vertices;
        std::vector<u16> indices;
    };

    void AddTriangle(const Vertex& a, const Vertex& b, const Vertex& c);
    void AddQuad(const Vertex& a, const Vertex& b, const Vertex& c, const Vertex& d);
    void AddTexturedQuad(const Poly_FT4& poly, const std::shared_ptr<SDLGPUTexture>& texture);
    void UploadBatch(Batch& batch);
    void DrawVertices(SDL_GPURenderPass* renderPass, Batch& batch);
    std::shared_ptr<SDLGPUTexture> PrepareTexture(const Poly_FT4& poly);
    void UploadWhiteTexture();
    void CreatePipeline();
    void DumpFrameIfDue();

    SDLGPUContext mContext;
    SDL_GPUTexture* mSwapchainTexture = nullptr;
    SDL_GPUCommandBuffer* mCommandBuffer = nullptr;
    SDL_GPUShader* mVertexShader = nullptr;
    SDL_GPUShader* mFragmentShader = nullptr;
    SDL_GPUGraphicsPipeline* mPipeline = nullptr;
    SDL_GPUSampler* mSampler = nullptr;
    SDL_GPUTexture* mWhiteTexture = nullptr;
    SDL_GPUTransferBuffer* mVertexTransferBuffer = nullptr;
    SDL_GPUTransferBuffer* mIndexTransferBuffer = nullptr;
    SDL_GPUTransferBuffer* mTextureTransferBuffer = nullptr;
    std::vector<Batch> mBatches;
    std::vector<u8> mTexturePixels;
    SDL_Rect mClipRect = {};
    bool mWhiteTextureUploaded = false;
    bool mFrameDumped = false;
    Uint64 mRendererStartTicks = 0;
    u8 mClearR = 0;
    u8 mClearG = 0;
    u8 mClearB = 0;
};

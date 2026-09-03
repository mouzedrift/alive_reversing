#pragma once

#include <SDL3/SDL_gpu.h>
#include <cstddef>

class SDLGPUTexture final
{
public:
    SDLGPUTexture() = default;
    SDLGPUTexture(SDL_GPUDevice* device, Uint32 width, Uint32 height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage);
    ~SDLGPUTexture();

    SDLGPUTexture(const SDLGPUTexture&) = delete;
    SDLGPUTexture& operator=(const SDLGPUTexture&) = delete;

    SDLGPUTexture(SDLGPUTexture&& other) noexcept;
    SDLGPUTexture& operator=(SDLGPUTexture&& other) noexcept;

    SDL_GPUTexture* Get() const { return mTexture; }
    Uint32 Width() const { return mWidth; }
    Uint32 Height() const { return mHeight; }

    void Upload(SDL_GPUCommandBuffer* commandBuffer, const void* pixels, Uint32 pitch);

    void Reset();

private:
    SDL_GPUDevice* mDevice = nullptr;
    SDL_GPUTexture* mTexture = nullptr;
    Uint32 mWidth = 0;
    Uint32 mHeight = 0;
};

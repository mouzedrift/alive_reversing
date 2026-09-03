#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

class SDLGPUContext final
{
public:
    explicit SDLGPUContext(SDL_Window* window);
    ~SDLGPUContext();

    SDL_GPUDevice* Device() const { return mDevice; }
    SDL_GPUCommandBuffer* BeginFrame(SDL_GPUTexture** swapchainTexture);
    void EndFrame(SDL_GPUCommandBuffer* commandBuffer);
    SDL_GPUTextureFormat SwapchainFormat() const { return mSwapchainFormat; }

private:
    SDL_Window* mWindow = nullptr;
    SDL_GPUDevice* mDevice = nullptr;
    SDL_GPUTextureFormat mSwapchainFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
};

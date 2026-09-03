#include "SDLGPUContext.hpp"

#include "../../FatalError.hpp"

SDLGPUContext::SDLGPUContext(SDL_Window* window)
    : mWindow(window)
{
    mDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
    if (!mDevice)
    {
        ALIVE_FATAL("Could not create SDL GPU device: %s", SDL_GetError());
    }

    if (!SDL_ClaimWindowForGPUDevice(mDevice, mWindow))
    {
        SDL_DestroyGPUDevice(mDevice);
        mDevice = nullptr;
        ALIVE_FATAL("Could not claim window for SDL GPU device: %s", SDL_GetError());
    }

    SDL_SetGPUSwapchainParameters(mDevice, mWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    mSwapchainFormat = SDL_GetGPUSwapchainTextureFormat(mDevice, mWindow);
}

SDLGPUContext::~SDLGPUContext()
{
    if (mDevice)
    {
        SDL_WaitForGPUIdle(mDevice);
        SDL_ReleaseWindowFromGPUDevice(mDevice, mWindow);
        SDL_DestroyGPUDevice(mDevice);
    }
}

SDL_GPUCommandBuffer* SDLGPUContext::BeginFrame(SDL_GPUTexture** swapchainTexture)
{
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(mDevice);
    if (!commandBuffer)
    {
        return nullptr;
    }

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, mWindow, swapchainTexture, nullptr, nullptr))
    {
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return nullptr;
    }

    return commandBuffer;
}

void SDLGPUContext::EndFrame(SDL_GPUCommandBuffer* commandBuffer)
{
    SDL_SubmitGPUCommandBuffer(commandBuffer);
}

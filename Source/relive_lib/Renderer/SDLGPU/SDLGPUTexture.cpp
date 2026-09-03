#include "SDLGPUTexture.hpp"
#include <cstring>

SDLGPUTexture::SDLGPUTexture(SDL_GPUDevice* device, Uint32 width, Uint32 height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage)
    : mDevice(device), mWidth(width), mHeight(height)
{
    SDL_GPUTextureCreateInfo info = {};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    mTexture = SDL_CreateGPUTexture(mDevice, &info);
}

SDLGPUTexture::~SDLGPUTexture()
{
    Reset();
}

SDLGPUTexture::SDLGPUTexture(SDLGPUTexture&& other) noexcept
    : mDevice(other.mDevice), mTexture(other.mTexture), mWidth(other.mWidth), mHeight(other.mHeight)
{
    other.mDevice = nullptr;
    other.mTexture = nullptr;
}

SDLGPUTexture& SDLGPUTexture::operator=(SDLGPUTexture&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        mDevice = other.mDevice;
        mTexture = other.mTexture;
        mWidth = other.mWidth;
        mHeight = other.mHeight;
        other.mDevice = nullptr;
        other.mTexture = nullptr;
    }
    return *this;
}

void SDLGPUTexture::Reset()
{
    if (mDevice && mTexture)
    {
        SDL_ReleaseGPUTexture(mDevice, mTexture);
    }
    mTexture = nullptr;
}

void SDLGPUTexture::Upload(SDL_GPUCommandBuffer* commandBuffer, const void* pixels, Uint32 pitch, Uint32 bytesPerPixel)
{
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = pitch * mHeight;
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(mDevice, &transferInfo);
    if (!transfer)
    {
        return;
    }

    void* destinationPixels = SDL_MapGPUTransferBuffer(mDevice, transfer, true);
    std::memcpy(destinationPixels, pixels, pitch * mHeight);
    SDL_UnmapGPUTransferBuffer(mDevice, transfer);

    SDL_GPUTextureTransferInfo source = {};
    source.transfer_buffer = transfer;
    source.pixels_per_row = pitch / bytesPerPixel;
    source.rows_per_layer = mHeight;
    SDL_GPUTextureRegion destination = {};
    destination.texture = mTexture;
    destination.w = mWidth;
    destination.h = mHeight;
    destination.d = 1;

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    SDL_UploadToGPUTexture(copyPass, &source, &destination, true);
    SDL_EndGPUCopyPass(copyPass);
    SDL_ReleaseGPUTransferBuffer(mDevice, transfer);
}

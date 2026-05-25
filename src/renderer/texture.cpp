// src/renderer/texture.cpp
#include "renderer/texture.h"

// stb_image: define IMPLEMENTATION in exactly one .cpp.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void Texture::generateCheckerboard(uint8_t* dst, int w, int h) {
    const int tile = 32;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const bool even = ((x / tile) + (y / tile)) % 2 == 0;
            const size_t idx = static_cast<size_t>((y * w + x) * 4);
            dst[idx + 0] = even ? 230 : 50;
            dst[idx + 1] = even ? 200 : 100;
            dst[idx + 2] = even ? 50 : 210;
            dst[idx + 3] = 255;
        }
    }
}

void Texture::loadFromFileOrCheckerboard(const VulkanContext* ctx, const ResourceFactory* resources,
                                         const std::string& path) {
    ctx_ = ctx;
    int texW = 0, texH = 0, texCh = 0;
    uint8_t* stbiPix = stbi_load(path.c_str(), &texW, &texH, &texCh, STBI_rgb_alpha);
    std::vector<uint8_t> owned;
    const uint8_t* pixels = stbiPix;
    if (!pixels) {
        std::cerr << "[Texture] file not found, falling back to checkerboard: " << path << "\n";
        texW = texH = 256;
        owned.resize(static_cast<size_t>(texW * texH * 4));
        generateCheckerboard(owned.data(), texW, texH);
        pixels = owned.data();
    } else {
        std::cout << "[Texture] loaded from file: " << path << " (" << texW << "x" << texH
                  << ")\n";
    }
    buildFromRgbaPixels(resources, pixels, texW, texH, true);
    if (stbiPix) stbi_image_free(stbiPix);
}

void Texture::loadFromMemory(const VulkanContext* ctx, const ResourceFactory* resources,
                             const uint8_t* encodedData, size_t size, bool srgb) {
    ctx_ = ctx;
    int texW = 0, texH = 0, texCh = 0;
    uint8_t* stbiPix = nullptr;
    std::vector<uint8_t> owned;
    const uint8_t* pixels = nullptr;
    if (encodedData && size > 0) {
        stbiPix = stbi_load_from_memory(encodedData, static_cast<int>(size), &texW, &texH, &texCh,
                                        STBI_rgb_alpha);
        pixels = stbiPix;
    }
    if (!pixels) {
        std::cerr << "[Texture] loadFromMemory failed, falling back to checkerboard\n";
        texW = texH = 256;
        owned.resize(static_cast<size_t>(texW * texH * 4));
        generateCheckerboard(owned.data(), texW, texH);
        pixels = owned.data();
    }
    buildFromRgbaPixels(resources, pixels, texW, texH, srgb);
    if (stbiPix) stbi_image_free(stbiPix);
}

void Texture::buildFromRgbaPixels(const ResourceFactory* resources, const uint8_t* pixels, int w,
                                  int h, bool srgb) {
    createImageAndView(resources, pixels, w, h, srgb);
    createSampler();
}

void Texture::loadFromRawRGBA(const VulkanContext* ctx, const ResourceFactory* resources,
                              const uint8_t* rgba, int width, int height) {
    ctx_ = ctx;
    if (!rgba || width <= 0 || height <= 0) {
        std::cerr << "[Texture] loadFromRawRGBA: invalid args, using checkerboard\n";
        int texW = 64, texH = 64;
        std::vector<uint8_t> owned(static_cast<size_t>(texW) * texH * 4);
        generateCheckerboard(owned.data(), texW, texH);
        createImageAndView(resources, owned.data(), texW, texH, true);
        createSampler();
        return;
    }
    createImageAndView(resources, rgba, width, height, true);
    createSampler();
}

void Texture::createImageAndView(const ResourceFactory* resources, const uint8_t* pixels, int width,
                                 int height, bool srgb) {
    // Color textures (albedo) are sRGB; data textures (normal/MR/AO) must be linear.
    const VkFormat fmt = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width * height * 4);

    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    resources->createBuffer(
        imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
        stagingMem);

    void* data = nullptr;
    vkMapMemory(ctx_->device(), stagingMem, 0, imageSize, 0, &data);
    std::memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(ctx_->device(), stagingMem);

    // Create image + memory into raw locals, then take ownership via VkUnique.
    // Move-assign frees any previous handle first, so re-loading a Texture is safe.
    // image memory is now VMA-managed via VmaImage::createAttachment.
    // ctx_ is const here; createAttachment needs non-const (allocator()), so cast.
    image_ = VmaImage::createAttachment(const_cast<VulkanContext*>(ctx_),
                                        static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                        fmt,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    resources->transitionImageLayout(image_.image(), VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    resources->copyBufferToImage(staging, image_.image(), static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height));
    resources->transitionImageLayout(image_.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx_->device(), staging, nullptr);
    vkFreeMemory(ctx_->device(), stagingMem, nullptr);

    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = image_.image();
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = fmt;
    ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(ctx_->device(), &ci, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Texture::createImageAndView: vkCreateImageView failed");
    view_ = VkUnique<VkImageView>(ctx_->device(), view);
}

void Texture::createSampler() {
    VkSamplerCreateInfo ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_LINEAR;
    ci.addressModeU = ci.addressModeV = ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ci.anisotropyEnable = VK_FALSE;
    ci.maxAnisotropy = 1.f;
    ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(ctx_->device(), &ci, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("Texture::createSampler: vkCreateSampler failed");
    sampler_ = VkUnique<VkSampler>(ctx_->device(), sampler);
}

void Texture::destroy() {
    // VkUnique frees each handle (no-op if empty). Reset in reverse dependency
    // order; the auto destructor would do the same if destroy() were never called.
    sampler_.reset();
    view_.reset();
    image_.reset();
    ctx_ = nullptr;
}

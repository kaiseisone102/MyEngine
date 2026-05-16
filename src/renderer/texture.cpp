// src/renderer/texture.cpp

#include "renderer/texture.h"

// stb_image: IMPLEMENTATION マクロを 1 つの .cpp でだけ定義する（二重定義厳禁）
// Step 4 で vulkan_renderer.cpp からここに移動した
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>
#include <stdexcept>
#include <vector>

#include "renderer/resource_factory.h"
#include "renderer/vulkan_context.h"

void Texture::loadFromFileOrCheckerboard(const VulkanContext* ctx, const ResourceFactory* resources,
                                         const std::string& path) {
    ctx_ = ctx;

    int texW = 0, texH = 0, texCh = 0;
    uint8_t* stbiPix = stbi_load(path.c_str(), &texW, &texH, &texCh, STBI_rgb_alpha);
    std::vector<uint8_t> owned;
    const uint8_t* pixels = stbiPix;

    if (!pixels) {
        // フォールバック: 256x256 の黄×青チェッカーボード
        texW = texH = 256;
        const int tile = 32;
        owned.resize(static_cast<size_t>(texW * texH * 4));
        for (int y = 0; y < texH; ++y) {
            for (int x = 0; x < texW; ++x) {
                const bool even = ((x / tile) + (y / tile)) % 2 == 0;
                const size_t idx = static_cast<size_t>((y * texW + x) * 4);
                owned[idx + 0] = even ? 230 : 50;
                owned[idx + 1] = even ? 200 : 100;
                owned[idx + 2] = even ? 50 : 210;
                owned[idx + 3] = 255;
            }
        }
        pixels = owned.data();
    }

    createImageAndView(resources, pixels, texW, texH);
    createSampler();

    if (stbiPix) stbi_image_free(stbiPix);
}

void Texture::createImageAndView(const ResourceFactory* resources, const uint8_t* pixels, int width,
                                 int height) {
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width * height * 4);

    // ステージングバッファを用意してピクセルを書き込む
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

    // DEVICE_LOCAL な最終画像を作成
    resources->createImage(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                           VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image_, memory_);

    // レイアウト遷移 → コピー → レイアウト遷移 の 3 ステップ
    resources->transitionImageLayout(image_, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    resources->copyBufferToImage(staging, image_, static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height));
    resources->transitionImageLayout(image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx_->device(), staging, nullptr);
    vkFreeMemory(ctx_->device(), stagingMem, nullptr);

    // ImageView
    VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = image_;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = VK_FORMAT_R8G8B8A8_SRGB;
    ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(ctx_->device(), &ci, nullptr, &view_) != VK_SUCCESS)
        throw std::runtime_error("Texture::createImageAndView: vkCreateImageView failed");
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
    if (vkCreateSampler(ctx_->device(), &ci, nullptr, &sampler_) != VK_SUCCESS)
        throw std::runtime_error("Texture::createSampler: vkCreateSampler failed");
}

void Texture::destroy() {
    if (!ctx_) return;
    if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(ctx_->device(), sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
    }
    if (view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx_->device(), view_, nullptr);
        view_ = VK_NULL_HANDLE;
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx_->device(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->device(), memory_, nullptr);
        memory_ = VK_NULL_HANDLE;
    }
    ctx_ = nullptr;
}
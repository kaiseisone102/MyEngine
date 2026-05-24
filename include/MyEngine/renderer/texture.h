// include/MyEngine/renderer/texture.h
#pragma once
// =============================================================================
// Texture - GPU 2D texture (image + memory + view + sampler) + bindless index.
// =============================================================================
// Rule of Zero: each GPU handle is a VkUnique<...> member, so Texture declares
// NO destructor / copy / move. Compiler-generated move is correct and noexcept
// (so std::vector<Texture> moves, not copies, on reallocation), copy is
// implicitly deleted (handles are non-copyable), and destruction frees every
// handle via member destructors. The old hand-written moveFrom() pitfall
// (forget a field -> silent leak) is gone.
//
//   loadFromFileOrCheckerboard(path)
//   loadFromMemory(encodedBytes, size)   -- PNG/JPEG/... via stb_image
//   loadFromRawRGBA(rgba, w, h)          -- raw RGBA8, no decode (procedural)
// =============================================================================
#include <cstddef>
#include <cstdint>
#include <string>

#include "renderer/vk_unique.h"

class VulkanContext;
class ResourceFactory;

class Texture {
   public:
    Texture() = default;
    // Rule of Zero: no ~Texture / copy / move declared. VkUnique members make
    // Texture move-only and self-freeing automatically.

    void loadFromFileOrCheckerboard(const VulkanContext* ctx, const ResourceFactory* resources,
                                    const std::string& path);
    void loadFromMemory(const VulkanContext* ctx, const ResourceFactory* resources,
                        const uint8_t* encodedData, size_t size);
    // Phase 1F: build directly from raw RGBA8 pixels (w*h*4 bytes, no decoding).
    void loadFromRawRGBA(const VulkanContext* ctx, const ResourceFactory* resources,
                         const uint8_t* rgba, int width, int height);

    // Free the GPU handles now. Kept for explicit shutdown ordering (callers free
    // while the device is still alive). Safe to call multiple times; the auto
    // destructor would free anyway.
    void destroy();

    VkImageView view() const { return view_.get(); }
    VkSampler sampler() const { return sampler_.get(); }

    // === Phase 1D: bindless texture index ===
    uint32_t bindlessIndex() const { return bindlessIndex_; }
    void setBindlessIndex(uint32_t idx) { bindlessIndex_ = idx; }

   private:
    const VulkanContext* ctx_ = nullptr;
    VkUnique<VkImage> image_;
    VkUnique<VkDeviceMemory> memory_;
    VkUnique<VkImageView> view_;
    VkUnique<VkSampler> sampler_;
    uint32_t bindlessIndex_ = UINT32_MAX;

    void createImageAndView(const ResourceFactory* resources, const uint8_t* pixels, int width,
                            int height);
    void createSampler();
    void buildFromRgbaPixels(const ResourceFactory* resources, const uint8_t* pixels, int w, int h);
    static void generateCheckerboard(uint8_t* dst, int w, int h);
};
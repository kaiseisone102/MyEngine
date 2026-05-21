// include/MyEngine/renderer/texture.h
#pragma once
// =============================================================================
// Texture — Phase 1-D 段階2-c で loadFromMemory を追加
// =============================================================================
//   - 既存: loadFromFileOrCheckerboard(path)
//   - 新規: loadFromMemory(encodedBytes, size)  ← glTF 埋め込みテクスチャ用
//
// loadFromMemory は stb_image::stbi_load_from_memory を使うため、
// PNG/JPEG/BMP 等のエンコード済みバイト列を渡す前提。
// 生 RGBA バイト列をそのまま GPU に流す API は今回は作らない。
// =============================================================================

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <string>

class VulkanContext;
class ResourceFactory;

class Texture {
   public:
    void loadFromFileOrCheckerboard(const VulkanContext* ctx, const ResourceFactory* resources,
                                    const std::string& path);

    // メモリ上のエンコード済み画像 (PNG/JPEG等) からロードする。
    // size = エンコード済みバイト数。デコード失敗時はチェッカーボードにフォールバック。
    void loadFromMemory(const VulkanContext* ctx, const ResourceFactory* resources,
                        const uint8_t* encodedData, size_t size);

    // Phase 1F: build a texture directly from raw RGBA8 pixels (procedural).
    // rgba = width*height*4 bytes, no decoding. Used for generated grass etc.
    void loadFromRawRGBA(const VulkanContext* ctx, const ResourceFactory* resources,
                         const uint8_t* rgba, int width, int height);

    void destroy();

    VkImageView view() const { return view_; }
    VkSampler sampler() const { return sampler_; }

    // === Phase 1D: bindless texture index ===
    uint32_t bindlessIndex() const { return bindlessIndex_; }
    void setBindlessIndex(uint32_t idx) { bindlessIndex_ = idx; }

   private:
    const VulkanContext* ctx_ = nullptr;

    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    uint32_t bindlessIndex_ = UINT32_MAX;

    void createImageAndView(const ResourceFactory* resources, const uint8_t* pixels, int width,
                            int height);
    void createSampler();

    // 共通: 与えられた RGBA8 ピクセルから image/view/sampler を作る。
    void buildFromRgbaPixels(const ResourceFactory* resources, const uint8_t* pixels, int w, int h);

    // フォールバックチェッカーボード生成 (256x256, 32px tile)
    static void generateCheckerboard(uint8_t* dst, int w, int h);
};

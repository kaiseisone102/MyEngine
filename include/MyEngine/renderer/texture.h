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
    Texture() = default;
    ~Texture() { destroy(); }

    // GPU resources are owned; copying would double-free. Move transfers ownership.
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept { moveFrom(other); }
    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) { destroy(); moveFrom(other); }
        return *this;
    }

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
    // Transfer GPU handles from other, leaving it null so its destructor is a no-op.
    // NOTE: if you add a member to Texture, add it here too (manual move is a known
    //       pitfall; see MyEngine_VulkanHandle_Wrapper_Refactor.md for the Rule-of-Zero plan).
    void moveFrom(Texture& other) noexcept {
        ctx_ = other.ctx_;
        image_ = other.image_;
        memory_ = other.memory_;
        view_ = other.view_;
        sampler_ = other.sampler_;
        bindlessIndex_ = other.bindlessIndex_;
        other.ctx_ = nullptr;
        other.image_ = VK_NULL_HANDLE;
        other.memory_ = VK_NULL_HANDLE;
        other.view_ = VK_NULL_HANDLE;
        other.sampler_ = VK_NULL_HANDLE;
        other.bindlessIndex_ = UINT32_MAX;
    }

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

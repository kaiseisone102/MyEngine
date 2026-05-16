// include/MyEngine/renderer/skin_buffer_pool.h
#pragma once
// =============================================================================
// skin_buffer_pool.h — Phase 4 段階4-1
// =============================================================================
// 役割:
//   複数のスケルタルアニメ対象 (Player + 敵 100 体) のボーン行列 SSBO を
//   1 個の大きなバッファにまとめて管理する。
//
// 設計:
//   - 1 つの SSBO が MAX_SKIN_ENTITIES × MAX_SKIN_BONES 個のボーン行列を保持
//   - Slot 単位 (= MAX_SKIN_BONES 個) で allocate/release
//   - 各エンティティは自分の Slot.boneOffset を push_constant 経由でシェーダに渡す
//   - シェーダ内: skin.boneMatrices[push.skinOffset + jointIndex]
//   - MAX_FRAMES_IN_FLIGHT 個のバッファをラウンドロビン (FrameUniforms と同様)
//
// メモリ容量:
//   既定値: 128 ボーン × 128 体 × 64 byte = 1 MB / frame
//   × MAX_FRAMES_IN_FLIGHT (2) = 2 MB 総量。100 体規模に十分対応可能。
//
// SSBO レイアウト:
//   set=2, binding=0 : readonly buffer SkinMatrices { mat4 boneMatrices[]; };
//   各 Slot は連続した MAX_SKIN_BONES 個の mat4 領域を持つ。
//
// パフォーマンス特性:
//   - DescriptorSet は Pool 全体で 1 個 (frame 毎)
//     -> 描画ループ内で descriptorSet 切替不要
//   - エンティティ毎の切替は push_constant (skinOffset の更新) のみ
//   - HOST_COHERENT なので flush 不要
// =============================================================================

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "frame_sync.h"

class VulkanContext;
class ResourceFactory;

class SkinBufferPool {
   public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = FrameSync::MAX_FRAMES_IN_FLIGHT;
    static constexpr uint32_t MAX_BONES_PER_ENTITY = 128;
    static constexpr uint32_t MAX_ENTITIES = 128;
    static constexpr uint32_t TOTAL_BONES = MAX_BONES_PER_ENTITY * MAX_ENTITIES;

    // Slot: 1 エンティティ分の領域を表す (allocate の戻り値)
    struct Slot {
        uint32_t boneOffset = 0;     // SSBO 内のボーン番号オフセット
        uint32_t boneCapacity = 0;   // 確保容量 (= MAX_BONES_PER_ENTITY)
        bool valid() const { return boneCapacity > 0; }
        // 無効な slot (allocate 失敗時など)
        static Slot invalid() { return Slot{0, 0}; }
    };

    void init(VulkanContext* ctx, ResourceFactory* resources);
    void shutdown();

    // 1 エンティティ分の Slot を確保。MAX_ENTITIES に達していたら invalid を返す。
    Slot allocate();

    // Slot を解放しフリーリストへ戻す。
    void release(Slot slot);

    // 現在フレームの SSBO に matrices を書き込む。
    // matrices.size() は MAX_BONES_PER_ENTITY 以下を想定。
    void update(uint32_t frameIndex, const Slot& slot, const std::vector<glm::mat4>& matrices);

    VkDescriptorSetLayout layout() const { return layout_; }
    VkDescriptorSet descriptorSet(uint32_t frameIndex) const { return sets_[frameIndex]; }

    // 現在使用中の slot 数 (デバッグ・診断用)
    uint32_t allocatedCount() const { return allocatedCount_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkDeviceSize bufferSize_ = 0;

    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets_{};

    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers_{};
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> memories_{};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> mapped_{};

    // フリーリスト: 各 slot index (0..MAX_ENTITIES-1) を LIFO で管理
    std::vector<uint32_t> freeSlots_;
    uint32_t allocatedCount_ = 0;

    void createLayout();
    void createPool();
    void createBuffers(ResourceFactory* resources);
    void allocateAndWriteSets();
    void initFreeList();
};

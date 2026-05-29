#pragma once
// =============================================================================
// particle_pass.h — GPU instancing で粒子配列を描画 + 距離フェード
// =============================================================================
// 1 quad (4 頂点) を GPU instancing で kMaxParticlesPerFrame 個まで描画。
// instance buffer は frames_in_flight 分の host visible mapped。
//
// 距離フェード:
//   fadeStart = cullingDistance * kFadeStartRatio
//   fadeEnd   = cullingDistance
//   shader 側で粒子の distance を計算し、 fadeStart〜fadeEnd で線形に
//   alpha を 1→0 へ補間。 cullingDistance <= 0 なら フェードなし。
// =============================================================================

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/particle.h"
#include "renderer/frame_sync.h"
#include "renderer/vk_unique.h"
#include "renderer/vma_buffer.h"

class VulkanContext;
class ResourceFactory;
class Swapchain;
class DeletionQueue;

class ParticlePass {
   public:
    // Initial per-frame particle capacity. Foundations \xc2\xa78.1 dynamic-growth:
    // when execute() sees more alive particles than capacity_, growToFit()
    // doubles capacity_, hands the in-flight instance VB pair to the
    // DeletionQueue, allocates a larger pair, and the write proceeds at
    // the new size. Initial value matches the historical kMaxParticlesPerFrame.
    static constexpr uint32_t INITIAL_CAPACITY = 2048;

    // \xe3\x83\x95\xe3\x82\xa7\xe3\x83\xbc\xe3\x83\x89\xe7\xaf\x84\xe5\x9b\xb2\xe3\x81\xae\xe6\xaf\x94\xe7\x8e\x87 (cullingDistance \xe3\x81\xab\xe5\xaf\xbe\xe3\x81\x99\xe3\x82\x8b\xe6\xaf\x94)\xe3\x80\x82
    // fadeStart = cullingDistance * kFadeStartRatio\xe3\x80\x81 fadeEnd = cullingDistance\xe3\x80\x82
    static constexpr float kFadeStartRatio = 0.6f;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        Swapchain* swapchain = nullptr;
        DeletionQueue* deletionQueue = nullptr;  // F4: grow path defers old VBs
        // PART4 4a-1: dynamic rendering. See debug_line_pass.h.
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkDescriptorSet frameSet = VK_NULL_HANDLE;
        uint32_t frameIndex = 0;
        const std::vector<particle::Particle>* particles = nullptr;
        // フェード適用される最大距離 (m)。
        // 0 以下なら フェードなし (全粒子フル表示)。
        float cullingDistance = -1.f;
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

   private:
    // push constant
    struct ParticlePC {
        float fadeStart;
        float fadeEnd;
        float _pad0;
        float _pad1;
    };

    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;
    Swapchain* swapchain_ = nullptr;
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_ = 0;

    VkUnique<VkPipelineLayout> layout_;
    VkUnique<VkPipeline> pipeline_;

    VmaBuffer quadVB_;
    VmaBuffer quadIB_;

    std::array<VmaBuffer, FrameSync::MAX_FRAMES_IN_FLIGHT> instanceVBs_{};

    // PART4 4a-1: dynamic rendering — formats instead of VkRenderPass.
    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;

    void createLayout(VkDescriptorSetLayout frameSetLayout);
    void createPipeline(const std::string& shaderDir);
    void createQuadBuffers();
    void createInstanceBuffers();
    void destroyBuffers();
    void growToFit(uint32_t requiredParticles);  // F4: double capacity_, replace VBs
};

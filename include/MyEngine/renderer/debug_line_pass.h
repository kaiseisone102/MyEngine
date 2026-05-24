#pragma once
// =============================================================================
// debug_line_pass.h — デバッグ線描画用 Vulkan Pass
// =============================================================================
// 設計:
//   - MainPass の renderPass を共有 (vkCmdBeginRenderPass は MainPass がやる、
//     その render pass の中で execute される)
//   - Pipeline 2 つ: LINE_LIST 用 と TRIANGLE_LIST 用
//   - 頂点バッファは per-frame に動的アップロード (固定サイズ、 vkMapMemory)
//   - 既存の frame set (set=0) を流用 (vp 行列を使う)
//   - PushConstants なし
//
// 描画特性:
//   - cullMode = NONE (両面)
//   - depthTest = TRUE (壁の向こうの線は描かない)
//   - depthWrite = FALSE (デバッグ描画は depth に影響しない)
//   - アルファブレンド有効 (扇形の半透明塗りつぶし用)
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"
#include "renderer/vma_buffer.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "renderer/debug_line_vertex.h"
#include "renderer/frame_sync.h"

class VulkanContext;
class ResourceFactory;
class Swapchain;

class DebugLinePass {
   public:
    // 1 フレームあたりの最大頂点数 (line + triangle 合計)。
    // 攻撃可視化の典型用途では 200 程度なので、 10000 で大幅余裕。
    // 1 頂点 28 bytes、 10000 頂点で 280KB/frame。
    static constexpr uint32_t kMaxVerticesPerFrame = 10000;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        Swapchain* swapchain = nullptr;
        VkRenderPass mainRenderPass = VK_NULL_HANDLE;  // MainPass の renderPass を共有
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkDescriptorSet frameSet = VK_NULL_HANDLE;
        uint32_t frameIndex = 0;
        const std::vector<DebugLineVertex>* lineVertices = nullptr;
        const std::vector<DebugLineVertex>* triVertices = nullptr;
    };

    void init(const InitInfo& info);
    void shutdown();

    // MainPass の vkCmdBeginRenderPass 内で呼ばれる。
    void execute(const ExecuteInfo& info);

   private:
    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;
    Swapchain* swapchain_ = nullptr;

    VkUnique<VkPipelineLayout> layout_;
    VkUnique<VkPipeline> linePipeline_;   // VK_PRIMITIVE_TOPOLOGY_LINE_LIST
    VkUnique<VkPipeline> triPipeline_;    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST

    // per-frame 頂点バッファ (line / triangle で別バッファ)
    std::array<VmaBuffer, FrameSync::MAX_FRAMES_IN_FLIGHT> lineVBs_{};

    std::array<VmaBuffer, FrameSync::MAX_FRAMES_IN_FLIGHT> triVBs_{};

    void createLayout(VkDescriptorSetLayout frameSetLayout);
    void createPipelines(VkRenderPass renderPass, const std::string& shaderDir);
    VkPipeline buildPipeline(VkRenderPass renderPass, const std::string& shaderDir,
                              VkPrimitiveTopology topology);
    void createVertexBuffers();
    void destroyVertexBuffers();
};

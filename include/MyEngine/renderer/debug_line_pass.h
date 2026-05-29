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
class DeletionQueue;

class DebugLinePass {
   public:
    // Initial per-frame vertex capacity. Foundations \xc2\xa78.1: starting size,
    // not an upper bound -- execute() doubles capacity_ and re-creates the
    // line + triangle VB pair via the DeletionQueue when the live count
    // exceeds it. Attack-visualisation use sits around 200 verts so 10000
    // is generous enough that growth basically never fires today.
    static constexpr uint32_t INITIAL_CAPACITY = 10000;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        Swapchain* swapchain = nullptr;
        DeletionQueue* deletionQueue = nullptr;  // F5: grow path defers old VBs
        // PART4 4a-1: main_pass uses Vulkan 1.3 dynamic rendering, so we no
        // longer share a VkRenderPass. The child pipeline is compatible by
        // matching color and depth attachment formats.
        VkFormat colorFormat = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
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
    DeletionQueue* dq_ = nullptr;
    uint32_t capacity_ = 0;

    VkUnique<VkPipelineLayout> layout_;
    VkUnique<VkPipeline> linePipeline_;   // VK_PRIMITIVE_TOPOLOGY_LINE_LIST
    VkUnique<VkPipeline> triPipeline_;    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST

    // per-frame 頂点バッファ (line / triangle で別バッファ)
    std::array<VmaBuffer, FrameSync::MAX_FRAMES_IN_FLIGHT> lineVBs_{};

    std::array<VmaBuffer, FrameSync::MAX_FRAMES_IN_FLIGHT> triVBs_{};

    void createLayout(VkDescriptorSetLayout frameSetLayout);
    // PART4 4a-1: dynamic rendering. Pipelines are built against attachment
    // formats (not a VkRenderPass).
    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    void createPipelines(const std::string& shaderDir);
    VkPipeline buildPipeline(const std::string& shaderDir, VkPrimitiveTopology topology);
    void createVertexBuffers();
    void destroyVertexBuffers();
    void growToFit(uint32_t requiredVertices);  // F5: double capacity_, replace VBs
};

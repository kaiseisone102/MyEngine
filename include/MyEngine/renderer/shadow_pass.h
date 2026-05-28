// include/MyEngine/renderer/shadow_pass.h
#pragma once
// =============================================================================
// shadow_pass.h — Phase 5-B 段階B-A
// =============================================================================
// Phase 5-B 追加:
//   ExecuteInfo::staticModelDrawList — 装備品など静的 Model の影を描画する。
//   既存の staticPipeline_ で描画 (新パイプライン追加なし)。
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"

#include <cstdint>
#include <glm/glm.hpp>
#include "shaders/shared/types.h"
#include <string>
#include <vector>

#include "render_target.h"
#include "scene/scene_data.h"

class VulkanContext;
class ResourceFactory;
class Mesh;
class Model;
class GeometryBuffer;

namespace static_cull { struct BlockRange; }

class ShadowPass {
   public:
    using SkinnedPushConstants = myengine::shared::ShadowSkinnedPushConstants;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
        VkExtent2D extent = {1024, 1024};
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkDescriptorSet frameSet = VK_NULL_HANDLE;
        // Phase 1B-4b: BDA address for skin matrices
        VkDeviceAddress skinAddress = 0;
        const Mesh* mesh = nullptr;
        const std::vector<MeshDrawItem>* meshDrawList = nullptr;
        const std::vector<SkinnedDrawItem>* modelDrawList = nullptr;
        // Phase 5-B: 装備品など静的 Model の影 (skinned shadow continues here)
        const std::vector<StaticModelDrawItem>* staticModelDrawList = nullptr;

        // PART4 4-前-5: GPU-driven static-mesh shadow. When all of these are
        // set, shadow_pass routes the cube / static-model draws through
        // indirect_exec (vkCmdDrawIndexedIndirectCount) backed by
        // CullingPass's shadow-set compactCmd / countBuf. Skinned shadow
        // stays on the legacy CPU loop above.
        const GeometryBuffer* geometry = nullptr;
        const static_cull::BlockRange* blockRanges = nullptr;
        uint32_t blockRangeCount = 0;
        VkBuffer compactCommandBuffer = VK_NULL_HANDLE;
        VkBuffer indirectCountBuffer = VK_NULL_HANDLE;
        VkDeviceAddress drawBufferAddress = 0;  // DrawDataPool BDA for shadow.vert
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

    const RenderTarget& output() const { return target_; }

   private:
    VulkanContext* ctx_ = nullptr;
    VkExtent2D extent_{};
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;

    RenderTarget target_;
    VkUnique<VkRenderPass> renderPass_;
    VkUnique<VkFramebuffer> framebuffer_;

    VkUnique<VkPipelineLayout> staticLayout_;
    VkUnique<VkPipeline> staticPipeline_;
    VkUnique<VkPipelineLayout> skinnedLayout_;
    VkUnique<VkPipeline> skinnedPipeline_;

    void createRenderPass();
    void createTarget(ResourceFactory* resources);
    void createFramebuffer();
    void createStaticPipeline(VkDescriptorSetLayout frameSetLayout,
                              const std::string& shaderDir);
    void createSkinnedPipeline(VkDescriptorSetLayout frameSetLayout,
                                const std::string& shaderDir);
};

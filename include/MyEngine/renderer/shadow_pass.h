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
        // Phase 5-B: 装備品など静的 Model の影
        const std::vector<StaticModelDrawItem>* staticModelDrawList = nullptr;
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
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;

    VkPipelineLayout staticLayout_ = VK_NULL_HANDLE;
    VkPipeline staticPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout skinnedLayout_ = VK_NULL_HANDLE;
    VkPipeline skinnedPipeline_ = VK_NULL_HANDLE;

    void createRenderPass();
    void createTarget(ResourceFactory* resources);
    void createFramebuffer();
    void createStaticPipeline(VkDescriptorSetLayout frameSetLayout,
                              const std::string& shaderDir);
    void createSkinnedPipeline(VkDescriptorSetLayout frameSetLayout,
                                const std::string& shaderDir);
};

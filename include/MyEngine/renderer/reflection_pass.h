#pragma once
// =============================================================================
// reflection_pass.h — 2B-3: + ExecuteInfo + execute 実装
// =============================================================================

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "core/game_settings.h"
#include "renderer/reflection_target.h"
#include "scene/scene_data.h"  // draw item types

class VulkanContext;
class ResourceFactory;
class Mesh;
class Model;

class ReflectionPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout materialSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout skinSetLayout = VK_NULL_HANDLE;
        uint32_t baseWidth = 1280;
        uint32_t baseHeight = 720;
        VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        ReflectionQuality quality = ReflectionQuality::Half;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        // 反射用 frameSet (= 反射 view matrix の UBO)
        VkDescriptorSet frameSet = VK_NULL_HANDLE;
        VkDescriptorSet defaultMaterialSet = VK_NULL_HANDLE;
        VkDescriptorSet skinSet = VK_NULL_HANDLE;
        const Mesh* mesh = nullptr;

        // 反射描画では opaque のみ使う (簡略化)
        const std::vector<MeshDrawItem>* meshDrawListOpaque = nullptr;
        const std::vector<SkinnedDrawItem>* modelDrawListOpaque = nullptr;
        const std::vector<StaticModelDrawItem>* staticModelDrawListOpaque = nullptr;
        const std::vector<TerrainDrawItem>* terrainDrawListOpaque = nullptr;

        // クリアカラー (= 空っぽい空色)
        glm::vec4 clearColor = {0.5f, 0.7f, 0.9f, 1.f};
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);

    void rebuild(ReflectionQuality quality, uint32_t baseWidth, uint32_t baseHeight);

    VkRenderPass renderPass() const { return renderPass_; }
    const ReflectionTarget& target() const { return target_; }
    bool enabled() const { return quality_ != ReflectionQuality::Off; }
    ReflectionQuality quality() const { return quality_; }

    VkPipeline staticPipeline() const { return staticPipeline_; }
    VkPipelineLayout staticLayout() const { return staticLayout_; }
    VkPipeline skinnedPipeline() const { return skinnedPipeline_; }
    VkPipelineLayout skinnedLayout() const { return skinnedLayout_; }

   private:
    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;

    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    ReflectionQuality quality_ = ReflectionQuality::Half;

    ReflectionTarget target_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;

    VkPipelineLayout staticLayout_ = VK_NULL_HANDLE;
    VkPipeline staticPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout skinnedLayout_ = VK_NULL_HANDLE;
    VkPipeline skinnedPipeline_ = VK_NULL_HANDLE;

    void createRenderPass();
    void createFramebuffer();
    void createStaticLayout(VkDescriptorSetLayout frameSetLayout,
                            VkDescriptorSetLayout materialSetLayout);
    void createSkinnedLayout(VkDescriptorSetLayout frameSetLayout,
                              VkDescriptorSetLayout materialSetLayout,
                              VkDescriptorSetLayout skinSetLayout);

    struct PipelineBuildArgs {
        VkPipelineLayout layout;
        const std::string& vertSpv;
        const std::string& fragSpv;
        bool skinned;
    };
    VkPipeline buildPipeline(const PipelineBuildArgs& args, const std::string& shaderDir);

    void destroyTargetAndFramebuffer();
};

#pragma once
// =============================================================================
// reflection_pass.h — 2B-3: + ExecuteInfo + execute 実装
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"

#include <string>
#include <vector>

#include "core/game_settings.h"
#include "renderer/reflection_target.h"
#include "scene/scene_data.h"  // draw item types

class VulkanContext;
class ResourceFactory;
class Mesh;
class Model;
class DrawDataPool;

class ReflectionPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout bindlessSetLayout = VK_NULL_HANDLE;  // S4-c: for static bindless
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
        VkDescriptorSet bindlessSet = VK_NULL_HANDLE;  // S4-c: bindless texture array for static
        // Phase 1B-4b: BDA address for skin matrices
        VkDeviceAddress skinAddress = 0;
        const Mesh* mesh = nullptr;
        VkDeviceAddress drawBufferAddress = 0;  // Phase 2B PART3b: DrawData SSBO (static draws)
        uint32_t frameIndex = 0;
        DrawDataPool* drawDataPool = nullptr;   // Phase 2B PART3b: per-frame pushOne target

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

    const ReflectionTarget& target() const { return target_; }
    bool enabled() const { return quality_ != ReflectionQuality::Off; }
    ReflectionQuality quality() const { return quality_; }

    VkPipeline staticPipeline() const { return staticPipeline_.get(); }
    VkPipelineLayout staticLayout() const { return staticLayout_.get(); }
    VkPipeline skinnedPipeline() const { return skinnedPipeline_.get(); }
    VkPipelineLayout skinnedLayout() const { return skinnedLayout_.get(); }

   private:
    VulkanContext* ctx_ = nullptr;
    ResourceFactory* resources_ = nullptr;

    VkFormat colorFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    ReflectionQuality quality_ = ReflectionQuality::Half;

    // PART4 4d: dynamic rendering migration. VkRenderPass + VkFramebuffer
    // removed; pipelines declare VkPipelineRenderingCreateInfo and execute()
    // wraps draws in vkCmdBeginRendering with color + depth attachments.
    ReflectionTarget target_;

    VkUnique<VkPipelineLayout> staticLayout_;
    VkUnique<VkPipeline> staticPipeline_;
    VkUnique<VkPipelineLayout> skinnedLayout_;
    VkUnique<VkPipeline> skinnedPipeline_;

    void createStaticLayout(VkDescriptorSetLayout frameSetLayout,
                            VkDescriptorSetLayout bindlessSetLayout);
    void createSkinnedLayout(VkDescriptorSetLayout frameSetLayout,
                              VkDescriptorSetLayout bindlessSetLayout);

    struct PipelineBuildArgs {
        VkPipelineLayout layout;
        const std::string& vertSpv;
        const std::string& fragSpv;
        bool skinned;
    };
    VkPipeline buildPipeline(const PipelineBuildArgs& args, const std::string& shaderDir);

    // PART4 4d: framebuffer is gone; only the target needs teardown now.
    void destroyTarget();
};

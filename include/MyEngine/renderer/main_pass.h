#pragma once
// =============================================================================
// main_pass.h ÃÂ¢ÃÂÃÂ + Water (useReflection + reflectVP)
// =============================================================================

#include <vulkan/vulkan.h>

#include "renderer/vk_unique.h"

#include <cstdint>
#include <glm/glm.hpp>
#include "shaders/shared/types.h"
#include <string>
#include <vector>

#include "scene/scene_data.h"

class VulkanContext;
class Swapchain;
class Mesh;
class Model;
class ImGuiLayer;
class DebugLinePass;
class DebugLineRenderer;
class ParticlePass;
namespace particle { struct Particle; }
class HudPass;
class HudDrawList;
class WaterPass;
class DrawDataPool;

class MainPass {
   public:
    using StaticPushConstants = myengine::shared::StaticPushConstants;

    using SkinnedPushConstants = myengine::shared::SkinnedPushConstants;
    // Phase 1D: bindless static draw with texture index in push constant
    using StaticBindlessPushConstants = myengine::shared::StaticBindlessPushConstants;
    using InstancedPushConstants = myengine::shared::InstancedPushConstants;
    using StaticDrawPC = myengine::shared::StaticDrawPushConstants;

    struct InitInfo {
        VulkanContext* ctx = nullptr;
        Swapchain* swapchain = nullptr;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        // === Phase 1H-2: HDR render target attachment ===
        VkImageView hdrColorView = VK_NULL_HANDLE;
        VkFormat hdrColorFormat = VK_FORMAT_UNDEFINED;
        // Phase 1D: bindless texture array set layout (set=1 in bindless pipeline)
        VkDescriptorSetLayout bindlessSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
        uint32_t frameIndex = 0;
        VkDescriptorSet frameSet = VK_NULL_HANDLE;
        // Phase 1D: bindless texture array descriptor set
        VkDescriptorSet bindlessSet = VK_NULL_HANDLE;
        bool drawBindlessTestCube = false;  // Step 1D-2c: test cube switch
        // Phase 1B-4b: BDA address for skin matrices (parallel to skinSet, replaces it in 1B-4c)
        VkDeviceAddress skinAddress = 0;
        const Mesh* mesh = nullptr;

        glm::vec4 clearColor{0.4f, 0.6f, 0.9f, 1.0f};

        const std::vector<MeshDrawItem>* meshDrawListOpaque = nullptr;
        VkDeviceAddress instanceBufferAddress = 0;
        VkDeviceAddress drawBufferAddress = 0;  // Phase 2B PART3b: DrawData SSBO (static draws)
        DrawDataPool* drawDataPool = nullptr;   // Phase 2B PART3b: per-frame pushOne target
        const std::vector<InstancedMeshDrawItem>* grassDrawList = nullptr;  // Phase 1F
        const std::vector<SkinnedDrawItem>* modelDrawListOpaque = nullptr;
        const std::vector<StaticModelDrawItem>* staticModelDrawListOpaque = nullptr;
        const std::vector<TerrainDrawItem>* terrainDrawListOpaque = nullptr;

        const std::vector<MeshDrawItem>* meshDrawListTransparent = nullptr;
        const std::vector<SkinnedDrawItem>* modelDrawListTransparent = nullptr;
        const std::vector<StaticModelDrawItem>* staticModelDrawListTransparent = nullptr;
        const std::vector<TerrainDrawItem>* terrainDrawListTransparent = nullptr;

        // Water
        WaterPass* waterPass = nullptr;
        const std::vector<WaterDrawItem>* waterDrawList = nullptr;
        float waterTime = 0.f;
        bool waterUseReflection = false;
        glm::mat4 waterReflectVP{1.f};

        ImGuiLayer* imgui = nullptr;
        DebugLinePass* debugLinePass = nullptr;
        const DebugLineRenderer* debugLines = nullptr;
        ParticlePass* particlePass = nullptr;
        const std::vector<particle::Particle>* particles = nullptr;
        float particleCullingDistance = 100.f;
        HudPass* hudPass = nullptr;
        const HudDrawList* hud = nullptr;
        float screenW = 0.f;
        float screenH = 0.f;
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);
    void onSwapchainResized();
    void setHdrColorView(VkImageView view) { hdrColorView_ = view; }  // Phase 1H-2

    VkRenderPass renderPass() const { return renderPass_.get(); }

   private:
    struct PipelineBuildArgs {
        VkPipelineLayout layout;
        std::string vertSpv;
        std::string fragSpv;
        bool transparent;
        bool isSkinned = false;  // Phase 1B-6: 6 attrs if true, 4 attrs if false
        bool noCull = false;     // Phase 1F: grass needs both faces
    };

    VulkanContext* ctx_ = nullptr;
    Swapchain* swapchain_ = nullptr;

    VkUnique<VkRenderPass> renderPass_;
    // Phase 1H-2: cached HDR target info
    VkImageView hdrColorView_ = VK_NULL_HANDLE;
    VkFormat hdrColorFormat_ = VK_FORMAT_UNDEFINED;
    std::vector<VkUnique<VkFramebuffer>> framebuffers_;

    VkUnique<VkPipelineLayout> staticLayout_;
    VkUnique<VkPipeline> staticPipelineOpaque_;
    VkUnique<VkPipeline> staticPipelineTransparent_;

    VkUnique<VkPipelineLayout> skinnedLayout_;
    VkUnique<VkPipeline> skinnedPipelineOpaque_;
    VkUnique<VkPipeline> skinnedPipelineTransparent_;

    // === Phase 1D: bindless pipeline (opaque only for now) ===
    VkUnique<VkPipelineLayout> bindlessLayout_;
    VkUnique<VkPipeline> bindlessPipelineOpaque_;
    // === Phase 1E: instanced pipeline (opaque) ===
    // === Phase 1F: grass pipeline (alpha-tested, bindless, no cull) ===
    VkUnique<VkPipelineLayout> grassLayout_;
    VkUnique<VkPipeline> grassPipeline_;

    void createRenderPass();
    void createStaticLayout(VkDescriptorSetLayout frameSetLayout,
                             VkDescriptorSetLayout bindlessSetLayout);
    void createSkinnedLayout(VkDescriptorSetLayout frameSetLayout,
                              VkDescriptorSetLayout bindlessSetLayout);
    void createBindlessLayout(VkDescriptorSetLayout frameSetLayout,
                              VkDescriptorSetLayout bindlessSetLayout);
    void createGrassLayout(VkDescriptorSetLayout frameSetLayout, VkDescriptorSetLayout bindlessSetLayout);
    VkPipeline buildPipeline(const PipelineBuildArgs& args, const std::string& shaderDir);
    void createFramebuffers();
    void destroyFramebuffers();
};

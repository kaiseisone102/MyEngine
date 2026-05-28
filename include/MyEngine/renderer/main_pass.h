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
class GeometryBuffer;
namespace static_cull { struct PreparedDraw; struct BlockRange; }

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
        // PART4 4a-1: dynamic rendering requires the VkImage for the post-pass
        // COLOR_ATTACHMENT_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL transition that
        // VkRenderPass used to do implicitly via finalLayout.
        VkImage hdrColorImage = VK_NULL_HANDLE;
        // PART4 4a-2: GBuffer attachments (location=1 normal, location=2 motion).
        // Opaque-pass MRT; non-opaque draws use a separate begin/end with HDR
        // only.
        VkImageView normalView = VK_NULL_HANDLE;
        VkImage normalImage = VK_NULL_HANDLE;
        VkFormat normalFormat = VK_FORMAT_UNDEFINED;
        VkImageView motionView = VK_NULL_HANDLE;
        VkImage motionImage = VK_NULL_HANDLE;
        VkFormat motionFormat = VK_FORMAT_UNDEFINED;
        // Phase 1D: bindless texture array set layout (set=1 in bindless pipeline)
        VkDescriptorSetLayout bindlessSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    // PART4 4c-C: two-pass orchestration. Single = legacy / shadow / today's
    // single MainPass.execute (opaque CLEAR + non-opaque draws). FirstOpaque
    // = pass1 of the two-pass occlusion sequence (opaque CLEAR, SKIP non-
    // opaque; depth + GBuffer attachments stay in COLOR/DEPTH_ATTACHMENT
    // afterwards). SecondAndNonOpaque = pass2 (opaque LOAD, then non-opaque
    // section as Single does, then the post-pass barrier handoff to
    // OverlayPass).
    enum class Pass : uint32_t {
        Single = 0,
        FirstOpaque = 1,
        SecondAndNonOpaque = 2,
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
        // PART4 4c-C: see Pass enum above. pass_chain uses Single for the
        // legacy / not-yet-two-pass path, FirstOpaque/SecondAndNonOpaque
        // when running cull pass1 + pass2 on different opaque batches.
        Pass pass = Pass::Single;

        glm::vec4 clearColor{0.4f, 0.6f, 0.9f, 1.0f};

        const std::vector<MeshDrawItem>* meshDrawListOpaque = nullptr;
        VkDeviceAddress instanceBufferAddress = 0;
        VkDeviceAddress drawBufferAddress = 0;  // Phase 2B PART3b: DrawData SSBO (static draws)
        DrawDataPool* drawDataPool = nullptr;   // Phase 2B PART3b: per-frame pushOne target
        const GeometryBuffer* geometry = nullptr;  // Phase 2B PART3c: block bind for opaque static
        const std::vector<static_cull::PreparedDraw>* preparedOpaque = nullptr;  // PART3c
        const std::vector<static_cull::BlockRange>* preparedOpaqueRanges = nullptr;  // PART4 4-前-1
        VkBuffer indirectCommandBuffer = VK_NULL_HANDLE;  // Phase 2B PART3c-2 fallback path
        // PART4 4-前-4: visible-only compact draw list + per-block visible counts.
        // main_pass hands these to indirect_exec which picks DGC / IndirectCount /
        // Legacy per device capability.
        VkBuffer compactCommandBuffer = VK_NULL_HANDLE;
        VkBuffer indirectCountBuffer = VK_NULL_HANDLE;
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

        // PART4 4a-2 redesign: HUD + ImGui are no longer drawn inside
        // main_pass. They live in PassChain's OverlayPass step, which runs
        // after main_pass has finished and transitioned the GBuffer
        // attachments to read-only layouts. This eliminates the mid-pass
        // barrier dance the original 4a-2 needed (and which TDR'd on first
        // launch) and lets the debug viewer sample GBuffer textures without
        // a feedback-loop hazard.
        DebugLinePass* debugLinePass = nullptr;
        const DebugLineRenderer* debugLines = nullptr;
        ParticlePass* particlePass = nullptr;
        const std::vector<particle::Particle>* particles = nullptr;
        float particleCullingDistance = 100.f;
    };

    void init(const InitInfo& info);
    void shutdown();
    void execute(const ExecuteInfo& info);
    void onSwapchainResized();
    void setHdrColorView(VkImageView view) { hdrColorView_ = view; }  // Phase 1H-2
    // PART4 4a-1: dynamic rendering needs the VkImage to issue the post-pass
    // layout transition that VkRenderPass used to do via finalLayout.
    void setHdrColorImage(VkImage image) { hdrColorImage_ = image; }
    // PART4 4a-2: GBuffer attachments. Set during init and on swapchain
    // resize; views/images recreate together so they always match the depth /
    // hdr extent.
    void setNormalAttachment(VkImageView view, VkImage image) {
        normalView_ = view;
        normalImage_ = image;
    }
    void setMotionAttachment(VkImageView view, VkImage image) {
        motionView_ = view;
        motionImage_ = image;
    }

    // PART4 4a-1: replace VkRenderPass exposure with format accessors. Child
    // passes (debug_line / hud / particle / water / imgui) need to know which
    // formats main_pass renders to so their pipelines are
    // dynamic-rendering compatible via VkPipelineRenderingCreateInfo. 4a-2
    // will extend hdrColorFormat() to a colorFormats() array (HDR + normal +
    // motion).
    VkFormat hdrColorFormat() const { return hdrColorFormat_; }
    VkFormat depthFormat() const { return depthFormat_; }

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

    // PART4 4a-1: VkRenderPass / VkFramebuffer removed. main_pass now uses
    // Vulkan 1.3 dynamic rendering (vkCmdBeginRendering + VkRenderingInfo).
    // Pipelines are created with VkPipelineRenderingCreateInfo describing the
    // attachment formats below.
    VkImageView hdrColorView_ = VK_NULL_HANDLE;
    VkImage hdrColorImage_ = VK_NULL_HANDLE;
    VkFormat hdrColorFormat_ = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    // PART4 4a-2: GBuffer attachments (location=1 normal, location=2 motion).
    VkImageView normalView_ = VK_NULL_HANDLE;
    VkImage normalImage_ = VK_NULL_HANDLE;
    VkFormat normalFormat_ = VK_FORMAT_UNDEFINED;
    VkImageView motionView_ = VK_NULL_HANDLE;
    VkImage motionImage_ = VK_NULL_HANDLE;
    VkFormat motionFormat_ = VK_FORMAT_UNDEFINED;

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

    void createStaticLayout(VkDescriptorSetLayout frameSetLayout,
                             VkDescriptorSetLayout bindlessSetLayout);
    void createSkinnedLayout(VkDescriptorSetLayout frameSetLayout,
                              VkDescriptorSetLayout bindlessSetLayout);
    void createBindlessLayout(VkDescriptorSetLayout frameSetLayout,
                              VkDescriptorSetLayout bindlessSetLayout);
    void createGrassLayout(VkDescriptorSetLayout frameSetLayout, VkDescriptorSetLayout bindlessSetLayout);
    VkPipeline buildPipeline(const PipelineBuildArgs& args, const std::string& shaderDir);
};

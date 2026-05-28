#pragma once
// =============================================================================
// pass_chain.h — ReflectionPass 統合版 + reflectShadows 対応
// =============================================================================

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "core/game_settings.h"
#include "core/particle.h"
#include "debug_line_pass.h"
#include "frame_uniforms.h"
#include "hud_pass.h"
#include "imgui_layer.h"
#include "main_pass.h"
#include "overlay_pass.h"
#include "gbuffer_debug_widget.h"
#include "instance_buffer_pool.h"
#include "draw_data_pool.h"
#include "culling_pass.h"
#include "frustum.h"
#include "post_pass.h"
#include "bloom_pass.h"
#include "particle_pass.h"
#include "reflection_pass.h"
#include "shadow_pass.h"
#include "water_pass.h"

class VulkanContext;
class ResourceFactory;
class Swapchain;
class SceneData;
class AssetRegistry;
class Mesh;
class Model;
class DebugLineRenderer;
class HudDrawList;
class DeletionQueue;

class PassChain {
   public:
    struct InitInfo {
        SDL_Window* window = nullptr;
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        Swapchain* swapchain = nullptr;
        FrameUniforms* frameUniforms = nullptr;
        AssetRegistry* assets = nullptr;
        DeletionQueue* deletionQueue = nullptr;  // PART4 4-前-3: CullingPass / DrawDataPool grow path
        // Phase 1D: bindless texture set layout
        VkDescriptorSetLayout bindlessSetLayout = VK_NULL_HANDLE;
        // Phase 1H-2: HDR target attachment for MainPass
        VkImageView hdrColorView = VK_NULL_HANDLE;
        VkImage hdrColorImage = VK_NULL_HANDLE;  // PART4 4a-1: dynamic-rendering layout transition
        VkFormat hdrColorFormat = VK_FORMAT_UNDEFINED;
        VkSampler hdrColorSampler = VK_NULL_HANDLE;  // Phase 1H-3
        // PART4 4a-2: GBuffer attachments written by main_pass's opaque pass.
        VkImageView normalView = VK_NULL_HANDLE;
        VkImage normalImage = VK_NULL_HANDLE;
        VkFormat normalFormat = VK_FORMAT_UNDEFINED;
        VkImageView motionView = VK_NULL_HANDLE;
        VkImage motionImage = VK_NULL_HANDLE;
        VkFormat motionFormat = VK_FORMAT_UNDEFINED;
        // Phase 1I: compute mip-chain bloom. BloomPass owns its own mip chain;
        // we only pass the format, base (mip0) extent, and mip count.
        VkFormat bloomFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        uint32_t bloomBaseWidth = 0;
        uint32_t bloomBaseHeight = 0;
        uint32_t bloomMaxMips = 6;
        std::string shaderDir;
        ReflectionQuality reflectionQuality = ReflectionQuality::Half;
        bool reflectShadows = true;
    };

    struct RecordInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
        uint32_t frameIndex = 0;
        const SceneData* scene = nullptr;
        const AssetRegistry* assets = nullptr;
        FrameUniforms* frameUniforms = nullptr;
        // Phase 1B-4b: BDA address for skin matrices
        VkDeviceAddress skinAddress = 0;
        // Phase 1D: bindless texture descriptor set
        VkDescriptorSet bindlessSet = VK_NULL_HANDLE;
        const DebugLineRenderer* debugLines = nullptr;
        const std::vector<particle::Particle>* particles = nullptr;
        const HudDrawList* hud = nullptr;
        float screenW = 0.f;
        float screenH = 0.f;
        float waterTime = 0.f;

        // 通常 view + lighting (反射 VP の計算と shadowStrength の調整)
        FrameUniforms::LightingUBO normalLighting{};

        ReflectionQuality reflectQuality = ReflectionQuality::Half;
        bool reflectShadows = true;
    };

    void init(const InitInfo& info);
    void shutdown();

    void beginUI();
    void endUI();
    void recordFrame(const RecordInfo& info);

    // Phase 1F: instanced culling stats (for debug HUD)
    int lastInstancedVisible() const { return lastInstancedVisible_; }
    int lastInstancedTotal() const { return lastInstancedTotal_; }
    // Phase 2B PART3c-2: GPU-driven prop cull stat (visible/total) for the HUD.
    // visible is the previous same-frame dispatch, read after the frame fence.
    int lastCullGpuVisible() const { return lastCullGpuVisible_; }
    int lastCullTotal() const { return lastCullTotal_; }
    // PART4 4a-1/4a-2: forwards new HDR view + image and GBuffer
    // normal/motion attachments after a swapchain rebuild. All targets are
    // recreated together by VulkanRenderer::createHdrTarget.
    struct ResizeInfo {
        VkImageView hdrColorView = VK_NULL_HANDLE;
        VkSampler hdrColorSampler = VK_NULL_HANDLE;
        VkImage hdrColorImage = VK_NULL_HANDLE;
        VkImageView normalView = VK_NULL_HANDLE;
        VkImage normalImage = VK_NULL_HANDLE;
        VkImageView motionView = VK_NULL_HANDLE;
        VkImage motionImage = VK_NULL_HANDLE;
        uint32_t bloomBaseW = 0;
        uint32_t bloomBaseH = 0;
    };
    void onSwapchainResized(const ResizeInfo& info);

    void onReflectionQualityChanged(ReflectionQuality quality);
    void setTonemapMode(int mode) { postPass_.setTonemapMode(mode); }
    void setBloomEnabled(bool b) { bloomEnabled_ = b; }
    void setGrassColorVariation(bool on) { grassColorVariation_ = on; }
    bool grassColorVariation() const { return grassColorVariation_; }
    void setGrassWind(bool on) { windEnabled_ = on; }
    bool grassWind() const { return windEnabled_; }

    void processEvent(const SDL_Event& e) { ImGuiLayer::processEvent(e); }

   private:
    ShadowPass shadowPass_;
    MainPass mainPass_;
    InstanceBufferPool instancePool_;  // Phase 1E
    DrawDataPool drawDataPool_;          // Phase 2B PART3b: per-draw SSBO for static draws
    CullingPass cullingPass_;           // Phase 2B PART2: GPU frustum culling
    bool grassColorVariation_ = true;  // Phase 1F: grass color variation toggle
    bool windEnabled_ = true;          // Phase 1F: grass wind sway toggle
    int lastInstancedVisible_ = 0;  // Phase 1F
    int lastInstancedTotal_ = 0;
    int lastCullGpuVisible_ = 0;  // Phase 2B PART3c-2: GPU-driven prop cull stat
    int lastCullTotal_ = 0;
    PostPass postPass_;  // Phase 1H-3
    BloomPass bloomPass_;  // Phase 1I
    // PART4 4a-2: HUD + ImGui overlay rendered after main_pass and before
    // bloom/post. Owns the dynamic-rendering scope; HudPass / ImGuiLayer
    // remain owned by PassChain (init/shutdown here) and are driven through
    // OverlayPass.execute()'s ExecuteInfo pointers.
    OverlayPass overlayPass_;
    bool bloomEnabled_ = true;
    DebugLinePass debugLinePass_;
    ParticlePass particlePass_;
    HudPass hudPass_;
    WaterPass waterPass_;
    ReflectionPass reflectionPass_;
    ImGuiLayer imgui_;

    Swapchain* swapchain_ = nullptr;
    VulkanContext* ctx_ = nullptr;  // PART4 4a-2: needed for the viewer sampler

    // PART4 4a-2: HDR target handles cached for OverlayPass and viewer
    // forwarding. Refreshed on init / onSwapchainResized.
    VkImageView hdrColorView_ = VK_NULL_HANDLE;
    VkImage hdrColorImage_ = VK_NULL_HANDLE;
    GBufferDebugWidget gbufferWidget_;
};

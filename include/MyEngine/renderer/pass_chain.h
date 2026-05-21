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
#include "instance_buffer_pool.h"
#include "frustum.h"
#include "post_pass.h"
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

class PassChain {
   public:
    struct InitInfo {
        SDL_Window* window = nullptr;
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        Swapchain* swapchain = nullptr;
        FrameUniforms* frameUniforms = nullptr;
        AssetRegistry* assets = nullptr;
        // Phase 1D: bindless texture set layout
        VkDescriptorSetLayout bindlessSetLayout = VK_NULL_HANDLE;
        // Phase 1H-2: HDR target attachment for MainPass
        VkImageView hdrColorView = VK_NULL_HANDLE;
        VkFormat hdrColorFormat = VK_FORMAT_UNDEFINED;
        VkSampler hdrColorSampler = VK_NULL_HANDLE;  // Phase 1H-3
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
    void onSwapchainResized(VkImageView hdrColorView = VK_NULL_HANDLE, VkSampler hdrColorSampler = VK_NULL_HANDLE);  // Phase 1H-2/3

    void onReflectionQualityChanged(ReflectionQuality quality);
    void setTonemapMode(int mode) { postPass_.setTonemapMode(mode); }
    void setGrassColorVariation(bool on) { grassColorVariation_ = on; }
    bool grassColorVariation() const { return grassColorVariation_; }
    void setGrassWind(bool on) { windEnabled_ = on; }
    bool grassWind() const { return windEnabled_; }

    void processEvent(const SDL_Event& e) { ImGuiLayer::processEvent(e); }

   private:
    ShadowPass shadowPass_;
    MainPass mainPass_;
    InstanceBufferPool instancePool_;  // Phase 1E
    bool grassColorVariation_ = true;  // Phase 1F: grass color variation toggle
    bool windEnabled_ = true;          // Phase 1F: grass wind sway toggle
    int lastInstancedVisible_ = 0;  // Phase 1F
    int lastInstancedTotal_ = 0;
    PostPass postPass_;  // Phase 1H-3
    DebugLinePass debugLinePass_;
    ParticlePass particlePass_;
    HudPass hudPass_;
    WaterPass waterPass_;
    ReflectionPass reflectionPass_;
    ImGuiLayer imgui_;

    Swapchain* swapchain_ = nullptr;
};

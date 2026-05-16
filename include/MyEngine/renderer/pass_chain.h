// include/MyEngine/renderer/pass_chain.h
#pragma once
// =============================================================================
// pass_chain.h — レンダーパス群と ImGui のオーケストレーション
// =============================================================================

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <functional>
#include <string>

#include "frame_uniforms.h"
#include "imgui_layer.h"
#include "main_pass.h"
#include "shadow_pass.h"

class VulkanContext;
class ResourceFactory;
class Swapchain;
class SceneData;
class AssetRegistry;
class Texture;
class Mesh;
class Model;

class PassChain {
   public:
    struct InitInfo {
        SDL_Window* window = nullptr;
        VulkanContext* ctx = nullptr;
        ResourceFactory* resources = nullptr;
        Swapchain* swapchain = nullptr;
        FrameUniforms* frameUniforms = nullptr;
        const Texture* defaultTexture = nullptr;
        std::string shaderDir;
    };

    struct RecordInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
        uint32_t frameIndex = 0;
        const SceneData* scene = nullptr;
        const AssetRegistry* assets = nullptr;
        FrameUniforms* frameUniforms = nullptr;
    };

    void init(const InitInfo& info);
    void shutdown();

    void beginUI();
    void endUI();
    void recordFrame(const RecordInfo& info);
    void onSwapchainResized();

    void processEvent(const SDL_Event& e) { ImGuiLayer::processEvent(e); }

   private:
    ShadowPass shadowPass_;
    MainPass mainPass_;
    ImGuiLayer imgui_;
};

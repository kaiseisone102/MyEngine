// include/MyEngine/renderer/main_pass.h
#pragma once
// =============================================================================
// main_pass.h — Phase 1-D 段階1
// =============================================================================
// メインカラー描画パス。
//
// ExecuteInfo (Phase 1-D 段階1):
//   - mesh + meshDrawList   : Cube で描画される対象 (地面/敵/アイテム等)
//   - model + modelDrawList : Knight (Model) で描画される対象 (Player等)
//   両方任意。null/空なら該当ループをスキップする。
// =============================================================================

#include <vulkan/vulkan.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class VulkanContext;
class Swapchain;
class Mesh;
class Model;
class ImGuiLayer;

class MainPass {
   public:
    struct InitInfo {
        VulkanContext* ctx = nullptr;
        Swapchain* swapchain = nullptr;
        VkDescriptorSetLayout frameSetLayout = VK_NULL_HANDLE;
        std::string shaderDir;
    };

    struct ExecuteInfo {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
        VkDescriptorSet frameSet = VK_NULL_HANDLE;
        const Mesh* mesh = nullptr;
        const std::vector<glm::mat4>* meshDrawList = nullptr;
        const Model* model = nullptr;
        const std::vector<glm::mat4>* modelDrawList = nullptr;
        ImGuiLayer* imgui = nullptr;
        glm::vec4 clearColor = {0.05f, 0.06f, 0.10f, 1.f};
    };

    void init(const InitInfo& info);
    void shutdown();

    void onSwapchainResized();
    void execute(const ExecuteInfo& info);

    VkRenderPass renderPass() const { return renderPass_; }

   private:
    VulkanContext* ctx_ = nullptr;
    Swapchain* swapchain_ = nullptr;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    void createRenderPass();
    void createPipeline(VkDescriptorSetLayout frameSetLayout, const std::string& shaderDir);
    void createFramebuffers();
    void destroyFramebuffers();
};

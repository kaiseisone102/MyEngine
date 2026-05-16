// include/MyEngine/renderer/imgui_layer.h
#pragma once
// =============================================================================
// imgui_layer.h — リファクタ Step 7
//   Dear ImGui の Vulkan backend ライフサイクルを集約。
//
//   モダンエンジン同様、ImGui はメインパスの最後に乗せる「レイヤー」として
//   独立コンポーネント化する。RenderPass への依存は InitInfo で受け取る。
//
//   フレームのライフサイクル:
//     beginFrame() -> (ユーザーが ImGui で UI 構築) -> endFrame() -> recordDrawCommands(cmd)
//
//   SDL3 イベントは static processEvent(e) で流す (init/shutdown と独立)。
// =============================================================================

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <cstdint>

class VulkanContext;

class ImGuiLayer {
   public:
    struct InitInfo {
        SDL_Window* window = nullptr;
        VulkanContext* ctx = nullptr;
        uint32_t swapchainImageCount = 0;
        VkRenderPass renderPass = VK_NULL_HANDLE;  // ImGui を乗せる RenderPass (通常 MainPass)
        uint32_t minImageCount = 2;
    };

    void init(const InitInfo& info);
    void shutdown();

    // フレーム冒頭で呼ぶ (Vulkan/SDL3/ImGui の NewFrame をまとめる)。
    void beginFrame();

    // ユーザーが ImGui で UI を組んだ後、コマンド記録の前に呼ぶ。
    void endFrame();

    // メインパスの RenderPass 内で呼ぶ。endFrame() で生成された draw data を記録する。
    void recordDrawCommands(VkCommandBuffer cmd);

    // SDL3 イベントを ImGui に流す。アプリの main loop から呼ぶ。
    // init 前/shutdown 後に呼ばれても安全 (Context が無ければ no-op)。
    static void processEvent(const SDL_Event& e);

   private:
    bool initialized_ = false;
};

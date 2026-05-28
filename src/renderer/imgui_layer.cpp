// \MyEngine\src\renderer\imgui_layer.cpp
// =============================================================================
// imgui_layer.cpp
// =============================================================================
// 実装内容:
//   Dear ImGui (1.92.8) の Vulkan + SDL3 backend ライフサイクル管理。
//
//   1 フレームの呼び出し順序:
//     beginFrame()
//       → ユーザーが ImGui::Begin/End で UI を組む
//     endFrame()
//       → メインパスの中で recordDrawCommands(cmd)
//
//   イベントは init/shutdown と独立した processEvent(e) で流す。
//
// ImGui 1.92.8 (2025/09/26 BREAKING CHANGE 後) 仕様メモ:
//   - ImGui_ImplVulkan_Init(InitInfo*) のシグネチャは引数 1 つだけ。
//   - InitInfo.RenderPass / Subpass / MSAASamples は非推奨。
//     代わりに InitInfo.PipelineInfoMain.RenderPass 等を使う。
//   - font texture は自動ロード (旧版の CreateFontsTexture 明示呼び出し不要)。
//   - ImGui_ImplSDL3_InitForVulkan は bool を返す (失敗チェック必要)。
//   - ImGui_ImplSDL3_ProcessEvent は const SDL_Event* を取る。
//
// 状態の扱い:
//   .h には bool initialized_ しか宣言が無いので、 descriptorPool / device は
//   この .cpp の匿名 namespace に file-local static として持つ。
//   ImGui Context はグローバルなので、 ImGuiLayer インスタンスはアプリ内で
//   1 つだけ存在する前提。
// =============================================================================
#include "renderer/imgui_layer.h"

#include "renderer/vulkan_context.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>

namespace {
// ImGuiLayer は 1 アプリで 1 つ前提なので state は file-local に持つ。
VkDevice g_device = VK_NULL_HANDLE;
VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;
}  // namespace

// =============================================================================
// init
// =============================================================================

void ImGuiLayer::init(const InitInfo& info) {
    if (initialized_) return;
    if (!info.window || !info.ctx || info.colorFormat == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("ImGuiLayer::init: invalid InitInfo");
    }

    // ─── ImGui Context ─────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // ─── SDL3 backend ──────────────────────────────────────────────
    if (!ImGui_ImplSDL3_InitForVulkan(info.window)) {
        ImGui::DestroyContext();
        throw std::runtime_error("ImGuiLayer::init: ImGui_ImplSDL3_InitForVulkan failed");
    }

    // ─── Vulkan backend 用の descriptor pool ───────────────────────
    // ImGui は font texture / ユーザー追加テクスチャで descriptor を必要とする。
    // 余裕を持って各タイプ 1000 個用意 (ImGui 公式 example に倣う)。
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
    };
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pci.maxSets = 1000 * static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
    pci.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
    pci.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(info.ctx->device(), &pci, nullptr, &g_descriptorPool) !=
        VK_SUCCESS) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGuiLayer::init: vkCreateDescriptorPool failed");
    }
    g_device = info.ctx->device();

    // ─── Vulkan backend 本体 (1.92.8 仕様) ──────────────────────────
    // 注意: 2025/09/26 の BREAKING CHANGE で
    //   InitInfo.RenderPass / Subpass / MSAASamples が
    //   InitInfo.PipelineInfoMain.RenderPass 等に移動した。
    ImGui_ImplVulkan_InitInfo ii{};
    ii.Instance = info.ctx->instance();
    ii.PhysicalDevice = info.ctx->physicalDevice();
    ii.Device = info.ctx->device();
    ii.QueueFamily = info.ctx->graphicsFamily();
    ii.Queue = info.ctx->graphicsQueue();
    ii.DescriptorPool = g_descriptorPool;
    ii.MinImageCount = info.minImageCount;
    ii.ImageCount = info.swapchainImageCount;
    ii.PipelineCache = VK_NULL_HANDLE;
    ii.Allocator = nullptr;
    ii.CheckVkResultFn = nullptr;
    // PART4 4a-1: main_pass uses Vulkan 1.3 dynamic rendering. Hand the
    // attachment formats to ImGui's pipeline.
    ii.UseDynamicRendering = true;

    ii.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
    ii.PipelineInfoMain.Subpass = 0;
    ii.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &info.colorFormat;
    ii.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = info.depthFormat;

    if (!ImGui_ImplVulkan_Init(&ii)) {
        vkDestroyDescriptorPool(g_device, g_descriptorPool, nullptr);
        g_descriptorPool = VK_NULL_HANDLE;
        g_device = VK_NULL_HANDLE;
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGuiLayer::init: ImGui_ImplVulkan_Init failed");
    }

    // font texture は 1.92 系では自動でアップロードされる
    // (旧版の ImGui_ImplVulkan_CreateFontsTexture / DestroyFontUploadObjects は不要)

    initialized_ = true;
}

// =============================================================================
// shutdown
// =============================================================================

void ImGuiLayer::shutdown() {
    if (!initialized_) return;

    // 注意: shutdown 前に caller (VulkanRenderer) が vkDeviceWaitIdle を呼ぶ前提。
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (g_descriptorPool != VK_NULL_HANDLE && g_device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_device, g_descriptorPool, nullptr);
        g_descriptorPool = VK_NULL_HANDLE;
    }
    g_device = VK_NULL_HANDLE;
    initialized_ = false;
}

// =============================================================================
// frame lifecycle
// =============================================================================

void ImGuiLayer::beginFrame() {
    if (!initialized_) return;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
    if (!initialized_) return;
    ImGui::Render();
}

void ImGuiLayer::recordDrawCommands(VkCommandBuffer cmd) {
    if (!initialized_) return;
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData) return;
    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
}

// =============================================================================
// static event processing
// =============================================================================

void ImGuiLayer::processEvent(const SDL_Event& e) {
    // ImGui Context が無い (init 前 / shutdown 後) なら no-op
    if (!ImGui::GetCurrentContext()) return;
    ImGui_ImplSDL3_ProcessEvent(&e);
}

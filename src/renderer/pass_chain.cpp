// src/renderer/pass_chain.cpp
#include "renderer/pass_chain.h"

#include <stdexcept>

#include "renderer/asset_registry.h"
#include "renderer/model.h"
#include "renderer/resource_factory.h"
#include "renderer/swapchain.h"
#include "renderer/vulkan_context.h"
#include "scene/scene_data.h"

void PassChain::init(const InitInfo& info) {
    if (!info.ctx || !info.resources || !info.swapchain || !info.frameUniforms ||
        !info.defaultTexture) {
        throw std::runtime_error("PassChain::init: required pointer is null");
    }

    // ─── ShadowPass ─────────────────────────────────────────────
    {
        ShadowPass::InitInfo si{};
        si.ctx = info.ctx;
        si.resources = info.resources;
        si.frameSetLayout = info.frameUniforms->layout();
        si.shaderDir = info.shaderDir;
        si.extent = {1024, 1024};
        si.depthFormat = VK_FORMAT_D32_SFLOAT;
        shadowPass_.init(si);
    }

    // ─── FrameUniforms との配線 ─────────────────────────────────
    info.frameUniforms->bindTexture(info.defaultTexture->view(), info.defaultTexture->sampler());
    info.frameUniforms->bindShadowMap(shadowPass_.output().view(), shadowPass_.output().sampler());
    info.frameUniforms->rebuildDescriptorSets();

    // ─── MainPass ───────────────────────────────────────────────
    {
        MainPass::InitInfo mi{};
        mi.ctx = info.ctx;
        mi.swapchain = info.swapchain;
        mi.frameSetLayout = info.frameUniforms->layout();
        mi.shaderDir = info.shaderDir;
        mainPass_.init(mi);
    }

    // ─── ImGuiLayer ─────────────────────────────────────────────
    {
        ImGuiLayer::InitInfo ii{};
        ii.window = info.window;
        ii.ctx = info.ctx;
        ii.swapchainImageCount = info.swapchain->imageCount();
        ii.renderPass = mainPass_.renderPass();
        ii.minImageCount = 2;
        imgui_.init(ii);
    }
}

void PassChain::shutdown() {
    imgui_.shutdown();
    mainPass_.shutdown();
    shadowPass_.shutdown();
}

void PassChain::beginUI() { imgui_.beginFrame(); }
void PassChain::endUI() { imgui_.endFrame(); }

void PassChain::recordFrame(const RecordInfo& info) {
    if (!info.scene || !info.assets || !info.frameUniforms || info.cmd == VK_NULL_HANDLE) {
        throw std::runtime_error("PassChain::recordFrame: invalid RecordInfo");
    }

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(info.cmd, &bi) != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed");

    const VkDescriptorSet frameSet = info.frameUniforms->descriptorSet(info.frameIndex);
    Mesh* mesh = const_cast<Mesh*>(&info.assets->defaultMesh());
    const Model* activeModel = info.assets->activeModel();  // null可
    const auto& drawList = info.scene->drawList();

    // 1) シャドウパス
    //    Phase 1 段階D 時点では ShadowPass は Mesh のみ対応。
    //    Model の影は Phase 1-D 完了後または別段階で対応。
    {
        ShadowPass::ExecuteInfo si{};
        si.cmd = info.cmd;
        si.frameSet = frameSet;
        si.mesh = mesh;
        si.drawList = &drawList;
        shadowPass_.execute(si);
    }

    // 2) メインパス
    //    activeModel があれば Model を優先描画、無ければ Mesh を描画。
    //    (MainPass 側の execute() で分岐する)
    {
        MainPass::ExecuteInfo mi{};
        mi.cmd = info.cmd;
        mi.imageIndex = info.imageIndex;
        mi.frameSet = frameSet;
        mi.mesh = mesh;
        mi.model = activeModel;
        mi.drawList = &drawList;
        mi.imgui = &imgui_;
        mainPass_.execute(mi);
    }

    if (vkEndCommandBuffer(info.cmd) != VK_SUCCESS)
        throw std::runtime_error("vkEndCommandBuffer failed");
}

void PassChain::onSwapchainResized() { mainPass_.onSwapchainResized(); }

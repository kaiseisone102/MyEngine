// =============================================================================
// game_over_layer.cpp
// + Phase 1C: buildScene moved to SceneRenderer::buildSceneData path
// + Skin matrices GPU upload (SkinBufferPool::update)
// =============================================================================
#define NOMINMAX
#include "loop/game_over_layer.h"

#include <imgui.h>

#include <iostream>

#include "core/components.h"
#include "core/game_state.h"
#include "loop/layer_factory.h"
#include "renderer/frame_sync.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/vulkan_renderer.h"
#include "scene/scene_data.h"
#include "scene/scene_renderer.h"
#include "world/stage_id.h"

GameOverLayer::GameOverLayer(SceneRenderer& renderer, VulkanRenderer& vulkan,
                              ILayerFactory& factory, GameState& state)
    : MenuLayerBase(renderer, vulkan), factory_(factory), state_(state) {}

GameOverLayer::~GameOverLayer() = default;

void GameOverLayer::onEnter() {
    std::cout << "[GameOverLayer] enter\n";
    setSelectedIndex(0);
}

void GameOverLayer::onExit() {
    std::cout << "[GameOverLayer] exit\n";
}

void GameOverLayer::handleConfirm(int selectedIndex, LayerCommands& cmds) {
    switch (selectedIndex) {
        case 0:
            std::cout << "[GameOverLayer] Continue -> GameplayLayer (Terminal)\n";
            cmds.requestReplace(factory_.createGameplayLayer(StageId::Terminal));
            return;
        case 1:
            std::cout << "[GameOverLayer] Title -> TitleLayer\n";
            cmds.requestReplace(factory_.createTitleLayer());
            return;
    }
}

void GameOverLayer::buildScene(SceneData& scene) {
    auto& wd = state_.worldState.data;
    const glm::vec3 cameraPos = wd.player.is_alive() && wd.player.has<CTransform>()
                                     ? wd.player.get<CTransform>().pos
                                     : glm::vec3{0.f};
    renderer().buildSceneData(wd, cameraPos, scene, state_.settings.drawDistance);

    // Upload skin matrices to GPU SSBO
    const uint32_t skinFi = static_cast<uint32_t>(skinFrameIndex_);
    wd.world.each([&](flecs::entity, const CSkeletalAnim& sa) {
        if (!sa.skinSlot.valid()) return;
        if (sa.skinMatrices.empty()) return;
        wd.vulkan.skinBufferPool().update(skinFi, sa.skinSlot, sa.skinMatrices);
    });

    skinFrameIndex_ = (skinFrameIndex_ + 1) % FrameSync::MAX_FRAMES_IN_FLIGHT;
}

void GameOverLayer::drawBackground(float winW, float winH) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.f, 0.f), ImVec2(winW, winH), IM_COL32(0, 0, 0, 180));
}

void GameOverLayer::drawExtraUI(float winW, float winH) {
    (void)winW;
    (void)winH;
}

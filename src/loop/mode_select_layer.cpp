// =============================================================================
// mode_select_layer.cpp 窶・Game Start 縺ｧ Terminal 繧ｹ繝・・繧ｸ繧帝幕縺・// =============================================================================
#define NOMINMAX
#include "loop/mode_select_layer.h"

#include <imgui.h>

#include <iostream>

#include "loop/layer_factory.h"
#include "world/stage_id.h"

ModeSelectLayer::ModeSelectLayer(SceneRenderer& renderer, VulkanRenderer& vulkan, ILayerFactory& factory)
    : MenuLayerBase(renderer, vulkan), factory_(factory) {}

ModeSelectLayer::~ModeSelectLayer() = default;

void ModeSelectLayer::onEnter() {
    std::cout << "[ModeSelectLayer] enter\n";
    setSelectedIndex(0);
}

void ModeSelectLayer::onExit() { std::cout << "[ModeSelectLayer] exit\n"; }

void ModeSelectLayer::handleConfirm(int selectedIndex, LayerCommands& cmds) {
    switch (selectedIndex) {
        case 0:  // Game Start 竊・Terminal 繧ｹ繝・・繧ｸ縺九ｉ髢句ｧ・            std::cout << "[ModeSelectLayer] Game Start 竊・replace with GameplayLayer (Terminal)\n";
            cmds.requestReplace(factory_.createGameplayLayer(StageId::Terminal));
            return;
        case 1:  // Settings
            std::cout << "[ModeSelectLayer] Settings 竊・push SettingsLayer\n";
            cmds.requestPush(factory_.createSettingsLayer());
            return;
        case 2:  // Quit
            std::cout << "[ModeSelectLayer] Quit 竊・application exit\n";
            cmds.requestQuit();
            return;
    }
}

void ModeSelectLayer::drawBackground(float winW, float winH) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.f, 0.f), ImVec2(winW, winH), IM_COL32(0, 0, 0, 255));
}

// =============================================================================
// graphics_settings_layer.cpp 窶・+ Reflection Quality + Reflection Shadows
// =============================================================================
#define NOMINMAX
#include "loop/graphics_settings_layer.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include "core/game_state.h"
#include "loop/layer_factory.h"
#include "scene/scene_renderer.h"
#include "renderer/vulkan_renderer.h"
GraphicsSettingsLayer::GraphicsSettingsLayer(SceneRenderer& renderer, VulkanRenderer& vulkan, GameState& state,
                                                ILayerFactory& factory)
    : MenuLayerBase(renderer, vulkan), state_(state), factory_(factory) {}
GraphicsSettingsLayer::~GraphicsSettingsLayer() = default;
void GraphicsSettingsLayer::onEnter() {
    std::cout << "[GraphicsSettingsLayer] enter\n";
    snapshot_ = state_.settings;
    hasUnsavedChanges_ = false;
    setSelectedIndex(kIdxDrawDistance);
}
void GraphicsSettingsLayer::onExit() {
    std::cout << "[GraphicsSettingsLayer] exit\n";
}
std::string GraphicsSettingsLayer::formatDistance(float v) const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%dm", static_cast<int>(v + 0.5f));
    return buf;
}
std::vector<MenuItem> GraphicsSettingsLayer::menuItems() const {
    const std::string saveLabel = hasUnsavedChanges_ ? "Save *" : "Save";
    const auto& s = state_.settings;
    return {
        MenuItem("Draw Distance", formatDistance(s.drawDistance)),
        MenuItem("Reflection Quality", reflectionQualityName(s.reflectionQuality)),
        MenuItem("Reflection: Shadows", s.reflectShadows ? "On" : "Off"),
        MenuItem(saveLabel),
        MenuItem("Back"),
    };
}
void GraphicsSettingsLayer::handleConfirm(int selectedIndex, LayerCommands& cmds) {
    switch (selectedIndex) {
        case kIdxSave:
            doSave();
            break;
        case kIdxBack:
            handleBack(cmds);
            break;
        case kIdxReflectShadows: {
            // Enter 縺ｧ繧ゅヨ繧ｰ繝ｫ蜿ｯ閭ｽ (蟾ｦ蜿ｳ縺ｨ蜷後§蜍穂ｽ・
            auto& s = state_.settings;
            s.reflectShadows = !s.reflectShadows;
            hasUnsavedChanges_ = true;
            break;
        }
        default:
            break;
    }
}
void GraphicsSettingsLayer::handleAdjust(int selectedIndex, int direction, LayerCommands& cmds) {
    (void)cmds;
    auto& s = state_.settings;
    bool changed = false;
    switch (selectedIndex) {
        case kIdxDrawDistance: {
            const float newVal = std::clamp(
                s.drawDistance + direction * GameSettings::kDrawDistanceStep,
                GameSettings::kMinDrawDistance, GameSettings::kMaxDrawDistance);
            if (newVal != s.drawDistance) {
                s.drawDistance = newVal;
                changed = true;
            }
            break;
        }
        case kIdxReflectQuality: {
            // Off (0) 竊・Quarter (1) 竊・Half (2) 竊・Full (3) 縺ｧ蠕ｪ迺ｰ
            int q = static_cast<int>(s.reflectionQuality);
            q += direction;
            if (q < 0) q = 3;
            if (q > 3) q = 0;
            const ReflectionQuality newQ = static_cast<ReflectionQuality>(q);
            if (newQ != s.reflectionQuality) {
                s.reflectionQuality = newQ;
                changed = true;
            }
            break;
        }
        case kIdxReflectShadows: {
            // 蟾ｦ蜿ｳ縺ｩ縺｡繧峨〒繧ゅヨ繧ｰ繝ｫ
            (void)direction;
            s.reflectShadows = !s.reflectShadows;
            changed = true;
            break;
        }
        default:
            break;
    }
    if (changed) hasUnsavedChanges_ = true;
}
void GraphicsSettingsLayer::handleBack(LayerCommands& cmds) {
    if (!hasUnsavedChanges_) {
        std::cout << "[GraphicsSettingsLayer] Back (no unsaved changes)\n";
        cmds.requestPop();
        return;
    }
    std::cout << "[GraphicsSettingsLayer] Back with unsaved changes -> confirm dialog\n";
    cmds.requestPush(factory_.createChoiceOverlay(
        "Discard unsaved changes?", {"Yes", "No"},
        [this](int idx, LayerCommands& c) {
            c.requestPop();
            if (idx == 0) {
                std::cout << "[GraphicsSettingsLayer] discarding unsaved changes\n";
                discardChanges();
                c.requestPop();
            } else {
                std::cout << "[GraphicsSettingsLayer] continue editing\n";
            }
        }));
}
void GraphicsSettingsLayer::doSave() {
    auto& s = state_.settings;

    // Draw distance (SceneRenderer 縺ｫ蜊ｳ譎る←逕ｨ縲・ParticleSystem 縺ｯ GameplayLayer
    // 邨檎罰縺ｧ豈弱ヵ繝ｬ繝ｼ繝蜿肴丐)
    // Phase 1C: setCullingDistance 廃止 (layer_stack が state.settings.drawDistance を参照)

    // Reflection quality: orchestrator 邨檎罰縺ｧ VulkanRenderer 縺ｫ莨昴∴繧・    // (snapshot 縺ｨ蟾ｮ蛻・′縺ゅｋ縺ｨ縺阪□縺・dirty 繝輔Λ繧ｰ遶九※繧九・辟｡鬧・↑ rebuild 髦ｲ豁｢)
    if (s.reflectionQuality != snapshot_.reflectionQuality) {
        s.reflectionDirty = true;
        std::cout << "[GraphicsSettingsLayer] reflectionQuality changed: "
                  << reflectionQualityName(snapshot_.reflectionQuality) << " -> "
                  << reflectionQualityName(s.reflectionQuality) << "\n";
    }

    // Reflection shadows 縺ｯ shader UBO 縺ｮ蛟､縺縺代↑縺ｮ縺ｧ繝輔Λ繧ｰ荳崎ｦ・(谺｡繝輔Ξ繝ｼ繝縺九ｉ蜿肴丐)
    if (s.reflectShadows != snapshot_.reflectShadows) {
        std::cout << "[GraphicsSettingsLayer] reflectShadows changed: "
                  << (snapshot_.reflectShadows ? "On" : "Off") << " -> "
                  << (s.reflectShadows ? "On" : "Off") << "\n";
    }

    s.persistDirty = true;
    snapshot_ = s;
    hasUnsavedChanges_ = false;
    std::cout << "[GraphicsSettingsLayer] saved\n";
}
void GraphicsSettingsLayer::discardChanges() {
    state_.settings = snapshot_;
    hasUnsavedChanges_ = false;
    std::cout << "[GraphicsSettingsLayer] reverted to snapshot\n";
}
void GraphicsSettingsLayer::drawBackground(float winW, float winH) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.f, 0.f), ImVec2(winW, winH), IM_COL32(0, 0, 0, 255));
}

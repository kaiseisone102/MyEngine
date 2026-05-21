// =============================================================================
// graphics_settings_layer.cpp  E+ Reflection Quality + Reflection Shadows
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
        MenuItem("Tonemapper", tonemapModeName(s.tonemapMode)),
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
            // Enter でもトグル可能 (左右と同じ動佁E
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
            // Off (0) ↁEQuarter (1) ↁEHalf (2) ↁEFull (3) で循環
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
        case kIdxTonemap: {
            // ACES (0) -> AgX (1) -> Khronos PBR (2) cycle
            int t = static_cast<int>(s.tonemapMode);
            t += direction;
            if (t < 0) t = 2;
            if (t > 2) t = 0;
            const TonemapMode newT = static_cast<TonemapMode>(t);
            if (newT != s.tonemapMode) {
                s.tonemapMode = newT;
                vulkan().setTonemapMode(newT);  // \u5373\u9069\u7528 (push constant)
                changed = true;
            }
            break;
        }
        case kIdxReflectShadows: {
            // 左右どちらでもトグル
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

    // Draw distance (SceneRenderer に即時適用、EParticleSystem は GameplayLayer
    // 経由で毎フレーム反映)
    // Phase 1C: setCullingDistance �p�~ (layer_stack �� state.settings.drawDistance ��Q��)

    // Reflection quality: orchestrator 経由で VulkanRenderer に伝えめE    // (snapshot と差刁E��あるときだぁEdirty フラグ立てる、E無駁E�� rebuild 防止)
    if (s.reflectionQuality != snapshot_.reflectionQuality) {
        s.reflectionDirty = true;
        std::cout << "[GraphicsSettingsLayer] reflectionQuality changed: "
                  << reflectionQualityName(snapshot_.reflectionQuality) << " -> "
                  << reflectionQualityName(s.reflectionQuality) << "\n";
    }

    // Reflection shadows は shader UBO の値だけなのでフラグ不要E(次フレームから反映)
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

// =============================================================================
// settings_layer.cpp  E+ Graphics サブメニュー
// =============================================================================
#define NOMINMAX
#include "loop/settings_layer.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <iostream>

#include "core/game_state.h"
#include "loop/layer_factory.h"

SettingsLayer::SettingsLayer(const LayerContext& ctx)
    : MenuLayerBase(ctx), state_(ctx.state), factory_(ctx.factory) {}

SettingsLayer::~SettingsLayer() = default;

void SettingsLayer::onEnter() {
    std::cout << "[SettingsLayer] enter\n";
    snapshot_ = state_.settings;
    hasUnsavedChanges_ = false;
    setSelectedIndex(kIdxBGM);
}

void SettingsLayer::onExit() {
    std::cout << "[SettingsLayer] exit\n";
}

std::string SettingsLayer::formatVolume(float v) const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(v * 100.f + 0.5f));
    return buf;
}

std::string SettingsLayer::formatSensitivity(float v) const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}

std::vector<MenuItem> SettingsLayer::menuItems() const {
    const std::string saveLabel = hasUnsavedChanges_ ? "Save *" : "Save";

    return {
        MenuItem("BGM Volume",        formatVolume(state_.settings.bgmVolume)),
        MenuItem("SFX Volume",        formatVolume(state_.settings.sfxVolume)),
        MenuItem("Mouse Sensitivity", formatSensitivity(state_.settings.mouseSensitivity)),
        MenuItem("Key Bindings"),
        MenuItem("Graphics"),
        MenuItem(saveLabel),
        MenuItem("Back"),
    };
}

void SettingsLayer::handleConfirm(int selectedIndex, LayerCommands& cmds) {
    switch (selectedIndex) {
        case kIdxKeyBindings:
            std::cout << "[SettingsLayer] Key Bindings -> push KeyConfigLayer\n";
            cmds.requestPush(factory_.createKeyConfigLayer());
            break;
        case kIdxGraphics:
            std::cout << "[SettingsLayer] Graphics -> push GraphicsSettingsLayer\n";
            cmds.requestPush(factory_.createGraphicsSettingsLayer());
            break;
        case kIdxSave:
            doSave();
            break;
        case kIdxBack:
            handleBack(cmds);
            break;
        default:
            break;
    }
}

void SettingsLayer::handleAdjust(int selectedIndex, int direction, LayerCommands& cmds) {
    (void)cmds;
    auto& s = state_.settings;

    bool changed = false;

    switch (selectedIndex) {
        case kIdxBGM: {
            const float newVal = std::clamp(
                s.bgmVolume + direction * GameSettings::kVolumeStep,
                GameSettings::kMinVolume, GameSettings::kMaxVolume);
            if (newVal != s.bgmVolume) {
                s.bgmVolume = newVal;
                changed = true;
            }
            break;
        }
        case kIdxSFX: {
            const float newVal = std::clamp(
                s.sfxVolume + direction * GameSettings::kVolumeStep,
                GameSettings::kMinVolume, GameSettings::kMaxVolume);
            if (newVal != s.sfxVolume) {
                s.sfxVolume = newVal;
                changed = true;
            }
            break;
        }
        case kIdxSensitivity: {
            const float newVal = std::clamp(
                s.mouseSensitivity + direction * GameSettings::kSensitivityStep,
                GameSettings::kMinSensitivity, GameSettings::kMaxSensitivity);
            if (newVal != s.mouseSensitivity) {
                s.mouseSensitivity = newVal;
                changed = true;
            }
            break;
        }
        default:
            break;
    }

    if (changed) {
        hasUnsavedChanges_ = true;
    }
}

void SettingsLayer::handleBack(LayerCommands& cmds) {
    if (!hasUnsavedChanges_) {
        std::cout << "[SettingsLayer] Back (no unsaved changes)\n";
        cmds.requestPop();
        return;
    }

    std::cout << "[SettingsLayer] Back with unsaved changes -> confirm dialog\n";
    cmds.requestPush(factory_.createChoiceOverlay(
        "Discard unsaved changes?", {"Yes", "No"},
        [this](int idx, LayerCommands& c) {
            c.requestPop();
            if (idx == 0) {
                std::cout << "[SettingsLayer] discarding unsaved changes\n";
                discardChanges();
                c.requestPop();
            } else {
                std::cout << "[SettingsLayer] continue editing\n";
            }
        }));
}

void SettingsLayer::doSave() {
    auto& s     = state_.settings;
    auto& sound = state_.worldState.systems.sound;
    auto& cam   = state_.worldState.systems.cameraSystem;

    sound.setBGMVolume(s.bgmVolume);
    sound.setSFXVolume(s.sfxVolume);
    cam.setSensitivity(s.mouseSensitivity);

    s.persistDirty = true;

    snapshot_ = s;
    hasUnsavedChanges_ = false;

    std::cout << "[SettingsLayer] saved (BGM=" << s.bgmVolume
              << " SFX=" << s.sfxVolume
              << " Sens=" << s.mouseSensitivity << ")\n";
}

void SettingsLayer::discardChanges() {
    state_.settings = snapshot_;
    hasUnsavedChanges_ = false;
    std::cout << "[SettingsLayer] reverted to snapshot\n";
}

void SettingsLayer::drawBackground(float winW, float winH) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.f, 0.f), ImVec2(winW, winH), IM_COL32(0, 0, 0, 255));
}

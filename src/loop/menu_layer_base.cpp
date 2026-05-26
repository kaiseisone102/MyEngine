// =============================================================================
// menu_layer_base.cpp
// + Phase 1C: コンストラクタ 2 引数化、 vulkan_.onResize() 直接呼び出し
// + Event 型名修正: MenuNavigateUpRequested 等 (event_bus.h の定義に合わせる)
// =============================================================================
#include "loop/menu_layer_base.h"

#include <imgui.h>

#include <SDL3/SDL.h>

#include "core/event_bus.h"
#include "renderer/vulkan_renderer.h"
#include "scene/scene_renderer.h"

MenuLayerBase::MenuLayerBase(const LayerContext& ctx)
    : renderer_(ctx.sceneRenderer), vulkan_(ctx.vulkan) {}

MenuLayerBase::~MenuLayerBase() = default;

void MenuLayerBase::handleEvents(const EventBus& events, LayerCommands& cmds) {
    const auto items = menuItems();
    const int count = static_cast<int>(items.size());

    if (count > 0) {
        if (findEvent<MenuNavigateUpRequested>(events)) {
            selectedIndex_ = (selectedIndex_ - 1 + count) % count;
            return;
        }
        if (findEvent<MenuNavigateDownRequested>(events)) {
            selectedIndex_ = (selectedIndex_ + 1) % count;
            return;
        }
        if (findEvent<MenuAdjustLeftRequested>(events)) {
            handleAdjust(selectedIndex_, -1, cmds);
            return;
        }
        if (findEvent<MenuAdjustRightRequested>(events)) {
            handleAdjust(selectedIndex_, +1, cmds);
            return;
        }
    }
    if (findEvent<MenuConfirmRequested>(events)) {
        handleConfirm(selectedIndex_, cmds);
        return;
    }
    if (findEvent<MenuBackRequested>(events)) {
        handleBack(cmds);
        return;
    }
    if (findEvent<QuitRequested>(events)) {
        cmds.requestQuit();
        return;
    }
    if (findEvent<WindowResizeRequested>(events)) {
        vulkan_.onResize();
        return;
    }
}

const char* MenuLayerBase::hintText() const {
    if (menuItems().empty()) {
        return "Press Enter to Continue    Esc: Back";
    }
    return "Up/Down: Navigate    Enter: Confirm    Esc: Back";
}

void MenuLayerBase::drawImGui() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float winW = viewport->Size.x;
    const float winH = viewport->Size.y;

    drawBackground(winW, winH);
    drawMenuHeader(winW, winH);

    const auto items = menuItems();
    drawMenuItems(winW, winH, items);
    drawMenuHint(winW, winH, !items.empty());
    drawExtraUI(winW, winH);
}

void MenuLayerBase::drawMenuHeader(float winW, float winH) const {
    ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.22f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##MenuHeader", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::SetWindowFontScale(headerFontScale());
    ImGui::Text("%s", headerText());
    ImGui::End();
}

void MenuLayerBase::drawMenuItems(float winW, float winH,
                                    const std::vector<MenuItem>& items) const {
    if (items.empty()) return;

    if (menuLayout() == MenuLayout::Horizontal) {
        ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##MenuItems", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::SetWindowFontScale(itemFontScale());
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (i > 0) ImGui::SameLine(0.0f, 40.0f);
            const bool selected = (i == selectedIndex_);
            const ImVec4 color =
                selected ? ImVec4(1.f, 0.85f, 0.3f, 1.f) : ImVec4(0.7f, 0.7f, 0.7f, 1.f);
            ImGui::TextColored(color, "%s", items[i].label.c_str());
        }
        ImGui::End();
    } else {
        ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##MenuItems", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::SetWindowFontScale(itemFontScale());
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const bool selected = (i == selectedIndex_);
            const ImVec4 color =
                selected ? ImVec4(1.f, 0.85f, 0.3f, 1.f) : ImVec4(0.7f, 0.7f, 0.7f, 1.f);
            if (items[i].rightText.empty()) {
                ImGui::TextColored(color, "%s", items[i].label.c_str());
            } else {
                ImGui::TextColored(color, "%-24s  %s", items[i].label.c_str(),
                                    items[i].rightText.c_str());
            }
        }
        ImGui::End();
    }
}

void MenuLayerBase::drawMenuHint(float winW, float winH, bool hasItems) const {
    (void)hasItems;
    ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.92f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##MenuHint", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.f), "%s", hintText());
    ImGui::End();
}

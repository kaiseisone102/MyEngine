// =============================================================================
// choice_overlay_layer.cpp  EHorizontal / Vertical レイアウト対応版
// =============================================================================
#define NOMINMAX
#include "loop/choice_overlay_layer.h"

#include <imgui.h>

#include <iostream>
#include <string>
#include <utility>

namespace {

// プロセス内のインスタンスごとに一意な ID を生成 (ImGui Window 名の衝突回避)
int generateInstanceId() {
    static int s_counter = 0;
    return ++s_counter;
}

}  // namespace

ChoiceOverlayLayer::ChoiceOverlayLayer(const LayerContext& ctx, std::string prompt,
                                         std::vector<std::string> choices, OnChoice onChoice,
                                         MenuLayout layout)
    : MenuLayerBase(ctx),
      prompt_(std::move(prompt)),
      choices_(std::move(choices)),
      onChoice_(std::move(onChoice)),
      layout_(layout),
      instanceId_(generateInstanceId()) {
    backdropWinId_ = "##ChoiceOverlayBackdrop_" + std::to_string(instanceId_);
    dialogWinId_   = "##ChoiceDialog_" + std::to_string(instanceId_);
}

ChoiceOverlayLayer::~ChoiceOverlayLayer() = default;

void ChoiceOverlayLayer::onEnter() {
    std::cout << "[ChoiceOverlayLayer] enter (prompt='" << prompt_
              << "', choices=" << choices_.size()
              << ", layout=" << (layout_ == MenuLayout::Vertical ? "Vertical" : "Horizontal")
              << ")\n";
    setSelectedIndex(0);
}

void ChoiceOverlayLayer::onExit() { std::cout << "[ChoiceOverlayLayer] exit\n"; }

std::vector<MenuItem> ChoiceOverlayLayer::menuItems() const {
    std::vector<MenuItem> items;
    items.reserve(choices_.size());
    for (const auto& c : choices_) {
        items.emplace_back(c);
    }
    return items;
}

void ChoiceOverlayLayer::handleConfirm(int selectedIndex, LayerCommands& cmds) {
    if (!onChoice_) {
        cmds.requestPop();
        return;
    }
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(choices_.size())) {
        cmds.requestPop();
        return;
    }
    std::cout << "[ChoiceOverlayLayer] confirm idx=" << selectedIndex << " ('"
              << choices_[selectedIndex] << "')\n";
    onChoice_(selectedIndex, cmds);
}

namespace {

std::string formatChoiceText(const std::string& label, bool selected) {
    return selected ? "> " + label + " <" : "  " + label + "  ";
}

constexpr float kChoiceSpacing   = 40.f;
constexpr float kHeaderFontScale = 2.0f;
constexpr float kItemFontScale   = 1.8f;

constexpr float kDialogBgAlpha = 1.00f;
constexpr float kBackdropAlpha = 140.f / 255.f;

}  // namespace

void ChoiceOverlayLayer::drawBackdrop(const ImVec2& viewport) {
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, kBackdropAlpha));
    ImGui::Begin(backdropWinId_.c_str(), nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void ChoiceOverlayLayer::drawHorizontal(const ImVec2& viewport) {
    ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f), ImGuiCond_Always,
                             ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(48.f, 32.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 16.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.11f, kDialogBgAlpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.78f, 0.80f, 0.88f, 1.f));

    ImGui::Begin(dialogWinId_.c_str(), nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    ImGui::SetWindowFontScale(kHeaderFontScale);
    const float promptW = ImGui::CalcTextSize(prompt_.c_str()).x;
    const float windowW = ImGui::GetWindowSize().x;
    ImGui::SetCursorPosX((windowW - promptW) * 0.5f);
    ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "%s", prompt_.c_str());

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetWindowFontScale(kItemFontScale);

    float totalChoiceW = 0.f;
    for (size_t i = 0; i < choices_.size(); ++i) {
        const std::string txt =
            formatChoiceText(choices_[i], i == static_cast<size_t>(selectedIndex()));
        totalChoiceW += ImGui::CalcTextSize(txt.c_str()).x;
        if (i + 1 < choices_.size()) totalChoiceW += kChoiceSpacing;
    }
    ImGui::SetCursorPosX((windowW - totalChoiceW) * 0.5f);

    for (int i = 0; i < static_cast<int>(choices_.size()); ++i) {
        if (i > 0) ImGui::SameLine(0.f, kChoiceSpacing);
        const bool selected = (i == selectedIndex());
        const ImVec4 color =
            selected ? ImVec4(1.f, 0.85f, 0.2f, 1.f) : ImVec4(0.6f, 0.6f, 0.6f, 1.f);
        const std::string txt = formatChoiceText(choices_[i], selected);
        ImGui::TextColored(color, "%s", txt.c_str());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetWindowFontScale(0.9f);
    const char* hint = "Left/Right: Select    Enter: Confirm    Esc: Cancel";
    const float hintW = ImGui::CalcTextSize(hint).x;
    ImGui::SetCursorPosX((windowW - hintW) * 0.5f);
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.6f, 1.f), "%s", hint);

    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

void ChoiceOverlayLayer::drawVertical(const ImVec2& viewport) {
    ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f), ImGuiCond_Always,
                             ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(48.f, 32.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 12.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.11f, kDialogBgAlpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.78f, 0.80f, 0.88f, 1.f));

    ImGui::Begin(dialogWinId_.c_str(), nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    // タイトル (中央揃え)
    ImGui::SetWindowFontScale(kHeaderFontScale);
    const float promptW = ImGui::CalcTextSize(prompt_.c_str()).x;
    const float windowW = ImGui::GetWindowSize().x;
    ImGui::SetCursorPosX((windowW - promptW) * 0.5f);
    ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), "%s", prompt_.c_str());

    ImGui::Spacing();
    ImGui::Spacing();

    // 選択肢 (縦並び、各項目を中央揃え)
    ImGui::SetWindowFontScale(kItemFontScale);
    for (int i = 0; i < static_cast<int>(choices_.size()); ++i) {
        const bool selected = (i == selectedIndex());
        const ImVec4 color =
            selected ? ImVec4(1.f, 0.85f, 0.2f, 1.f) : ImVec4(0.6f, 0.6f, 0.6f, 1.f);
        const std::string txt = formatChoiceText(choices_[i], selected);
        const float w = ImGui::CalcTextSize(txt.c_str()).x;
        ImGui::SetCursorPosX((windowW - w) * 0.5f);
        ImGui::TextColored(color, "%s", txt.c_str());
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::SetWindowFontScale(0.9f);
    const char* hint = "Up/Down: Select    Enter: Confirm    Esc: Cancel";
    const float hintW = ImGui::CalcTextSize(hint).x;
    ImGui::SetCursorPosX((windowW - hintW) * 0.5f);
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.6f, 1.f), "%s", hint);

    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

void ChoiceOverlayLayer::drawImGui() {
    const ImVec2 viewport = ImGui::GetMainViewport()->Size;
    drawBackdrop(viewport);

    if (layout_ == MenuLayout::Vertical) {
        drawVertical(viewport);
    } else {
        drawHorizontal(viewport);
    }
}

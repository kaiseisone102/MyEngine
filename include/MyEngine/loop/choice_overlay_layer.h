#pragma once
// =============================================================================
// choice_overlay_layer.h — 選択肢オーバーレイ Layer
// + Phase 1C: ImVec2 のために imgui.h を include
// + Phase 1C fix: 不在メンバ instanceId_/backdropWinId_/dialogWinId_ を追加
//                  (.cpp が使っているが .h に宣言がなかった)
// =============================================================================
#include <imgui.h>  // ImVec2

#include <functional>
#include <string>
#include <vector>

#include "loop/menu_layer_base.h"

class ChoiceOverlayLayer : public MenuLayerBase {
   public:
    using OnChoice = std::function<void(int idx, LayerCommands& cmds)>;

    ChoiceOverlayLayer(const LayerContext& ctx, std::string prompt,
                        std::vector<std::string> choices, OnChoice onChoice,
                        MenuLayout layout = MenuLayout::Horizontal);
    ~ChoiceOverlayLayer() override;

    void onEnter() override;
    void onExit() override;

    bool blocksUpdate() const override { return false; }
    bool blocksRender() const override { return false; }

    void drawImGui() override;
    const char* name() const override { return "ChoiceOverlay"; }

   protected:
    std::vector<MenuItem> menuItems() const override;
    MenuLayout menuLayout() const override { return layout_; }
    void handleConfirm(int selectedIndex, LayerCommands& cmds) override;
    const char* headerText() const override { return prompt_.c_str(); }

   private:
    std::string prompt_;
    std::vector<std::string> choices_;
    OnChoice onChoice_;
    MenuLayout layout_;

    int instanceId_;
    std::string backdropWinId_;
    std::string dialogWinId_;

    void drawHorizontal(const ImVec2& viewport);
    void drawVertical(const ImVec2& viewport);
    void drawBackdrop(const ImVec2& viewport);
};

#pragma once
// =============================================================================
// mode_select_layer.h  Eモード選択画面 Layer
// =============================================================================

#include "loop/menu_layer_base.h"

class ILayerFactory;

class ModeSelectLayer : public MenuLayerBase {
   public:
    explicit ModeSelectLayer(const LayerContext& ctx);
    ~ModeSelectLayer() override;

    void onEnter() override;
    void onExit() override;

    const char* name() const override { return "ModeSelect"; }

   protected:
    std::vector<MenuItem> menuItems() const override {
        return {"Game Start", "Settings", "Quit"};
    }
    void handleConfirm(int selectedIndex, LayerCommands& cmds) override;
    const char* headerText() const override { return "Select Mode"; }
    float headerFontScale() const override { return 2.5f; }

    void drawBackground(float winW, float winH) override;

   private:
    ILayerFactory& factory_;
};

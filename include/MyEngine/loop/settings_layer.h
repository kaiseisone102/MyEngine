#pragma once
// =============================================================================
// settings_layer.h  E設定画面 Layer (+ Graphics サブメニュー)
// =============================================================================

#include "core/game_settings.h"
#include "loop/menu_layer_base.h"

class GameState;
class ILayerFactory;

class SettingsLayer : public MenuLayerBase {
   public:
    explicit SettingsLayer(const LayerContext& ctx);
    ~SettingsLayer() override;

    void onEnter() override;
    void onExit() override;

    const char* name() const override { return "Settings"; }

   protected:
    std::vector<MenuItem> menuItems() const override;

    void handleConfirm(int selectedIndex, LayerCommands& cmds) override;
    void handleAdjust(int selectedIndex, int direction, LayerCommands& cmds) override;
    void handleBack(LayerCommands& cmds) override;

    const char* headerText() const override { return "Settings"; }
    float headerFontScale() const override { return 2.5f; }

    const char* hintText() const override {
        return "Up/Down: Navigate    Left/Right: Adjust    Enter: Save/Back    Esc: Back";
    }

    void drawBackground(float winW, float winH) override;

   private:
    GameState& state_;
    ILayerFactory& factory_;

    GameSettings snapshot_;
    bool hasUnsavedChanges_ = false;

    static constexpr int kIdxBGM         = 0;
    static constexpr int kIdxSFX         = 1;
    static constexpr int kIdxSensitivity = 2;
    static constexpr int kIdxKeyBindings = 3;
    static constexpr int kIdxGraphics    = 4;
    static constexpr int kIdxSave        = 5;
    static constexpr int kIdxBack        = 6;
    static constexpr int kItemCount      = 7;

    std::string formatVolume(float v) const;
    std::string formatSensitivity(float v) const;

    void doSave();
    void discardChanges();
};

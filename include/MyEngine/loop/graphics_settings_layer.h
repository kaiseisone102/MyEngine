#pragma once
// =============================================================================
// graphics_settings_layer.h  EグラフィチE��設定画面 Layer
// =============================================================================
// 仕槁E
//   - 全頁E�� Save するまでファイルにもシスチE��にも反映しなぁE//   - Save 頁E��で永続化 + 反映 (drawDistance ↁESceneRenderer、E//     reflectionQuality ↁEVulkanRenderer::setReflectionQuality)
//   - Back 時に未保存変更あれば ChoiceOverlay で破棁E��誁E// =============================================================================

#include "core/game_settings.h"
#include "loop/menu_layer_base.h"

class GameState;
class ILayerFactory;

class GraphicsSettingsLayer : public MenuLayerBase {
   public:
    GraphicsSettingsLayer(SceneRenderer& renderer, VulkanRenderer& vulkan, GameState& state,
                          ILayerFactory& factory);
    ~GraphicsSettingsLayer() override;

    void onEnter() override;
    void onExit() override;

    const char* name() const override { return "GraphicsSettings"; }

   protected:
    std::vector<MenuItem> menuItems() const override;

    void handleConfirm(int selectedIndex, LayerCommands& cmds) override;
    void handleAdjust(int selectedIndex, int direction, LayerCommands& cmds) override;
    void handleBack(LayerCommands& cmds) override;

    const char* headerText() const override { return "Graphics"; }
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

    static constexpr int kIdxDrawDistance     = 0;
    static constexpr int kIdxReflectQuality   = 1;
    static constexpr int kIdxReflectShadows   = 2;
    static constexpr int kIdxTonemap          = 3;
    static constexpr int kIdxGrassWind        = 4;
    static constexpr int kIdxSave             = 5;
    static constexpr int kIdxBack             = 6;

    std::string formatDistance(float v) const;

    void doSave();
    void discardChanges();
};

#pragma once
// =============================================================================
// graphics_settings_layer.h — グラフィック設定画面 Layer
// =============================================================================
// 仕様:
//   - 全項目、Save するまでファイルにもシステムにも反映しない
//   - Save 時に永続化 + 反映 (drawDistance → SceneRenderer、
//     reflectionQuality → VulkanRenderer::setReflectionQuality)
//   - Back 時に未保存変更があれば ChoiceOverlay で破棄確認
// =============================================================================

#include "core/game_settings.h"
#include "loop/menu_layer_base.h"

class GameState;
class ILayerFactory;

class GraphicsSettingsLayer : public MenuLayerBase {
   public:
    explicit GraphicsSettingsLayer(const LayerContext& ctx);
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
    static constexpr int kIdxShadowQuality    = 5;
    static constexpr int kIdxBloom            = 6;
    static constexpr int kIdxNormalMapping    = 7;
    static constexpr int kIdxMRMapping        = 8;
    static constexpr int kIdxSave             = 9;
    static constexpr int kIdxBack             = 10;

    std::string formatDistance(float v) const;

    void doSave();
    void discardChanges();
};

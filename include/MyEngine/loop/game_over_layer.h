#pragma once
// =============================================================================
// game_over_layer.h  EPlayer 死亡時�Eメニュー画面
// =============================================================================

#include <cstdint>

#include "loop/menu_layer_base.h"

class ILayerFactory;
class GameState;
class SceneData;

class GameOverLayer : public MenuLayerBase {
   public:
    explicit GameOverLayer(const LayerContext& ctx);
    ~GameOverLayer() override;

    void onEnter() override;
    void onExit() override;

    void buildScene(SceneData& scene) override;

    const char* name() const override { return "GameOver"; }

   protected:
    std::vector<MenuItem> menuItems() const override {
        return {"Continue", "Title"};
    }
    void handleConfirm(int selectedIndex, LayerCommands& cmds) override;
    void handleBack(LayerCommands& cmds) override {
        (void)cmds;
    }
    const char* headerText() const override { return "GAME OVER"; }
    float headerFontScale() const override { return 4.0f; }

    void drawBackground(float winW, float winH) override;
    void drawExtraUI(float winW, float winH) override;

   private:
    ILayerFactory& factory_;
    GameState& state_;
    uint32_t skinFrameIndex_ = 0;
};

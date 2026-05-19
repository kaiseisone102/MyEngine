#pragma once
// =============================================================================
// default_layer_factory.h
// + Phase 1C: render::SceneRenderer → SceneRenderer (グローバル namespace)
//   各 Layer に VulkanRenderer& も渡す必要があるが、 state_ から取得するため
//   factory のメンバ追加は不要。 .cpp 側でアクセス。
// =============================================================================

#include "loop/layer_factory.h"

class GameState;
class SceneRenderer;

class DefaultLayerFactory : public ILayerFactory {
   public:
    DefaultLayerFactory(GameState& state, SceneRenderer& renderer, float gravity,
                          float jumpSpeed)
        : state_(state), renderer_(renderer), gravity_(gravity), jumpSpeed_(jumpSpeed) {}

    std::unique_ptr<ILayer> createTitleLayer() override;
    std::unique_ptr<ILayer> createModeSelectLayer() override;
    std::unique_ptr<ILayer> createGameplayLayer(StageId initialStage) override;
    std::unique_ptr<ILayer> createGameOverLayer() override;
    std::unique_ptr<ILayer> createSettingsLayer() override;
    std::unique_ptr<ILayer> createKeyConfigLayer() override;
    std::unique_ptr<ILayer> createGraphicsSettingsLayer() override;

    std::unique_ptr<ILayer> createChoiceOverlay(
        std::string prompt, std::vector<std::string> choices,
        std::function<void(int idx, LayerCommands& cmds)> onChoice,
        MenuLayerBase::MenuLayout layout) override;

   private:
    GameState& state_;
    SceneRenderer& renderer_;
    float gravity_;
    float jumpSpeed_;
};

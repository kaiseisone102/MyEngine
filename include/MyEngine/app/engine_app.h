// /include/app/engine_app.h
#pragma once
// =============================================================================
// engine_app.h — 段階A2 (TitleLayer 対応)
// + Phase 1C: render::SceneRenderer → SceneRenderer (グローバル namespace)
//   include パス: renderer/scene_renderer.h → scene/scene_renderer.h
// =============================================================================
// 変更:
//   - DefaultLayerFactory を所有 (initGame 後に構築)
//   - orchestrator に ILayerFactory& として渡す
// =============================================================================
#include <memory>
#include "core/game_state.h"
#include "loop/default_layer_factory.h"
#include "loop/game_loop_orchestrator.h"
#include "scene/scene_renderer.h"
#include "world/world_builder.h"

class EngineApp {
   public:
    void run();

   private:
    static constexpr float kGravity = -25.f;
    static constexpr float kJumpSpeed = 10.f;

    GameState state_;
    GameLoopOrchestrator orchestrator_;
    WorldBuilder worldBuilder_;

    // VulkanRenderer 初期化後に構築
    std::unique_ptr<SceneRenderer> sceneRenderer_;
    std::unique_ptr<DefaultLayerFactory> layerFactory_;

    void initWindow();
    void initGame();
    void cleanup();
};

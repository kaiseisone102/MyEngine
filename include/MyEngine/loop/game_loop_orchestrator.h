#pragma once
// =============================================================================
// game_loop_orchestrator.h
// =============================================================================
// LayerStack(SceneRenderer&) コンストラクタ要求のため、 SceneRenderer& も受け取る。
// =============================================================================

#include "core/game_state.h"

class ILayerFactory;
class SceneRenderer;

class GameLoopOrchestrator {
   public:
    void run(GameState& s, ILayerFactory& layerFactory, SceneRenderer& sceneRenderer,
              float gravity, float jumpSpeed) const;
};

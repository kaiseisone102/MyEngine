#pragma once
// =============================================================================
// game_loop_orchestrator.h
// =============================================================================

#include "core/game_state.h"

class ILayerFactory;

class GameLoopOrchestrator {
   public:
    void run(GameState& s, ILayerFactory& layerFactory) const;
};

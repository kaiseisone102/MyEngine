// \MyEngine\include\MyEngine\loop\game_loop_orchestrator.h

#pragma once

#include "core/game_state.h"

class GameLoopOrchestrator {
   public:
    void run(GameState& s, float gravity, float jumpSpeed) const;
};

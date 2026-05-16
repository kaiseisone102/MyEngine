// /include/app/engine_app.h

#pragma once

#include "core/game_state.h"
#include "loop/game_loop_orchestrator.h"
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

    void initWindow();
    void initGame();
    void cleanup();
};

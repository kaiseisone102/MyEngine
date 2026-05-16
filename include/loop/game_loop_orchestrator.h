#pragma once

#include "/core/game_state.h"

class GameLoopOrchestrator {
public:
    void run(GameState& s, float gravity, float jumpSpeed) const;
};

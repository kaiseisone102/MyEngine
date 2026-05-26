#pragma once
// =============================================================================
// systems/anim_state_system.h
// =============================================================================
// 入力抽象化: update に const ActionState& 引数を追加。
//             SDL_GetKeyboardState 直読みを廃止。
// =============================================================================

class GameState;
struct ActionState;

class AnimStateSystem {
   public:
    // Phase 5-A 既存: ジャンプ・落下判定用の Y 速度閾値
    static constexpr float kJumpVelThreshold = 0.5f;
    static constexpr float kFallVelThreshold = -0.5f;

    void update(GameState& gameState, float dt, const ActionState& input) const;
};

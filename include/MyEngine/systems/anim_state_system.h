// include/MyEngine/systems/anim_state_system.h
#pragma once
// =============================================================================
// anim_state_system.h — Phase 3-A
// =============================================================================
// 各キャラの CTransform / CPhysics / CVelocity / CAttack / CHealth を読み取り、
// CAnimState.current を更新する。
//
// 判定ロジックは決定論的 (= 同じ入力なら同じ出力)。状態管理は内部に持たない。
//
// 呼び出し:
//   SimulationSystem::updatePlayer の早い段階で
//   ws.animStateSystem.update(s, dt) として呼ぶ。
//
// 拡張:
//   敵もアニメさせる時は同じシステムが処理する (CAnimState を持つ全エンティティ)。
//   現状は Player のみ CAnimState を持つので、Player だけ判定される。
// =============================================================================

class GameState;

class AnimStateSystem {
   public:
    void update(GameState& s, float dt) const;

    // しきい値 (デバッグや調整時に外から触れるよう public)
    // 速度がこの値以下なら "止まっている" とみなす (m/s)
    static constexpr float kIdleSpeedThreshold = 0.2f;
    // 上昇/下降を判定する Y速度のしきい値
    static constexpr float kJumpVelThreshold = 0.5f;
    static constexpr float kFallVelThreshold = -0.5f;
};

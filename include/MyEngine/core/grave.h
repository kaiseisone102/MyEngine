#pragma once
// =============================================================================
// core/grave.h — 状態を持つ墓
// =============================================================================
// 状態遷移:
//   Intact (無傷)
//     ├─ slash/smash → Damaged, spirit x 2
//     └─ smash_down → Destroyed, spirit x 5 (フェード→消滅)
//   Damaged (損壊)
//     ├─ slash/smash → 変化なし
//     └─ smash_down → Destroyed, spirit x 3 (フェード→消滅)
//   Destroyed
//     └─ 何もしない (= 攻撃判定対象外)
//
// 状態の見た目は今は何もしない (全部同じモデル grave_spirit.glb)。
// Destroyed になったら chest と同じくフェードアウト後に destruct。
// =============================================================================

#include "core/aabb.h"

struct CGrave {
    enum class State { Intact, Damaged, Destroyed };

    State state = State::Intact;
    float destroyedElapsed = 0.f;

    static constexpr float kFadeStartTime = 1.0f;
    static constexpr float kFadeDuration  = 2.0f;
    static constexpr float kDestructTime  = kFadeStartTime + kFadeDuration;
};

struct GraveTag {};

// include/MyEngine/core/anim_state.h
#pragma once
// =============================================================================
// anim_state.h — Phase 5-A
// =============================================================================
// Phase 5-A 追加:
//   HitReact : ダメージを受けてのけぞる短時間状態 (敵専用、将来 Player にも)
// =============================================================================

enum class AnimState {
    Idle,
    Walk,
    Run,
    Jump,
    Fall,
    Land,
    Slash,
    Smash,
    SmashDown,
    SpinAttack,
    Dead,
    HitReact,  // Phase 5-A: ヒットリアクション
};

inline const char* resolveAnimAssetName(AnimState s) {
    switch (s) {
        case AnimState::Idle:
            return "idle";
        case AnimState::Walk:
            return "walk";
        case AnimState::Run:
            return "run";
        case AnimState::Jump:
            return "jump";
        case AnimState::Fall:
            return "fall";
        case AnimState::Land:
            return "land";
        case AnimState::Slash:
            return "slash";
        case AnimState::Smash:
            return "smash";
        case AnimState::SmashDown:
            return "smashdown";
        case AnimState::SpinAttack:
            return "spin_attack";
        case AnimState::Dead:
            return "death";
        case AnimState::HitReact:
            return "hit_react";  // Phase 5-A
    }
    return "idle";
}

inline const char* animStateName(AnimState s) {
    switch (s) {
        case AnimState::Idle:
            return "Idle";
        case AnimState::Walk:
            return "Walk";
        case AnimState::Run:
            return "Run";
        case AnimState::Jump:
            return "Jump";
        case AnimState::Fall:
            return "Fall";
        case AnimState::Land:
            return "Land";
        case AnimState::Slash:
            return "Slash";
        case AnimState::Smash:
            return "Smash";
        case AnimState::SmashDown:
            return "SmashDown";
        case AnimState::SpinAttack:
            return "spinAttack";
        case AnimState::Dead:
            return "Dead";
        case AnimState::HitReact:
            return "HitReact";  // Phase 5-A
    }
    return "Unknown";
}

inline bool isAttackState(AnimState s) {
    return s == AnimState::Slash || s == AnimState::Smash || s == AnimState::SmashDown ||
           s == AnimState::SpinAttack;
}

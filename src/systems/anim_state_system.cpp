// =============================================================================
// anim_state_system.cpp — + Fall 遷移ヒステリシス
// =============================================================================
// 修正:
//   斜面を下ってる時、 プレイヤーが地面と離れたり接触したりして
//   Fall ⇄ Walk がチラチラ切り替わる問題。
//   解決: Fall に入るには「空中時間が kAirborneMinTime 以上連続」 が必要。
//   1-2 フレームの微妙な ground 離脱では Fall に遷移しない。
//   現在状態が Jump (= 意図して空中に居る) なら airTime 関係なく即 Fall 可。
// =============================================================================
#include "systems/anim_state_system.h"

#include <cmath>
#include <iostream>

#include "core/action_state.h"
#include "core/attack_def.h"
#include "core/components.h"
#include "core/game_state.h"
#include "renderer/asset_registry.h"

namespace {

bool hasMovementInput(const ActionState& input) {
    constexpr float kThreshold = 0.1f;
    return std::abs(input.moveX) > kThreshold || std::abs(input.moveZ) > kThreshold;
}

bool hasJumpInput(const ActionState& input) { return input.jumpHeld; }

bool hasRunModifier(const ActionState& input) { return input.sprint; }

AnimState decideAttackState(const CAttack& atk) {
    if (!atk.isActive() || !atk.def) return AnimState::Idle;
    return atk.def->animState;
}

// Fall 遷移ヒステリシス: 空中時間がこれ未満なら Fall に入らない (= 地上扱い継続)。
// 斜面下りの微小な ground 離脱を吸収する。
constexpr float kAirborneMinTime = 0.05f;

AnimState decideNormalState(const CPhysics& phys, const CVelocity& vel, const ActionState& input,
                            float airTime, AnimState currentState) {
    if (!phys.onGround) {
        // 現在 Jump 中 (= 意図して飛んだ) なら、 airTime 関係なくいつも通り Fall 判定
        const bool alreadyAirborne =
            (currentState == AnimState::Jump || currentState == AnimState::Fall);

        if (!alreadyAirborne && airTime < kAirborneMinTime) {
            // 微小な ground 離脱 → 地上扱い (Walk/Idle/Run 判定に流す)
            if (hasMovementInput(input)) {
                if (hasRunModifier(input)) return AnimState::Run;
                return AnimState::Walk;
            }
            return AnimState::Idle;
        }

        if (vel.y > AnimStateSystem::kJumpVelThreshold) return AnimState::Jump;
        if (vel.y < AnimStateSystem::kFallVelThreshold) return AnimState::Fall;
        return AnimState::Jump;
    }
    if (hasMovementInput(input)) {
        if (hasRunModifier(input)) return AnimState::Run;
        return AnimState::Walk;
    }
    return AnimState::Idle;
}

constexpr float kLandMinAirTime = 0.15f;

AnimState decideEnemyAnimState(const CEnemyAI& ai, float moveVelSq) {
    constexpr float kMoveThresholdSq = 0.01f;

    if (ai.isDying) return AnimState::Dead;
    if (ai.hitReactTimer > 0.f) return AnimState::HitReact;

    if (ai.state == EnemyState::Attack || ai.punchActive) {
        return AnimState::Slash;
    }
    if (ai.state == EnemyState::Cooldown) return AnimState::Idle;
    if (ai.isDown) return AnimState::Idle;
    if (ai.spawnStunTimer > 0.f) return AnimState::Idle;
    if (moveVelSq > kMoveThresholdSq) return AnimState::Walk;
    return AnimState::Idle;
}

void applyTransition(CAnimState& as, AnimState desired, const char* entityLabel) {
    if (desired != as.current) {
        as.previous = as.current;
        as.current = desired;
        as.stateTimer = 0.f;
        std::cout << "[AnimState] " << entityLabel << ": " << animStateName(as.previous) << " -> "
                  << animStateName(as.current) << "\n";
    }
}

}  // namespace

void AnimStateSystem::update(GameState& s, float dt, const ActionState& input) const {
    // =========================================================================
    // 1. Player の AnimState 判定
    // =========================================================================
    flecs::entity player = s.worldState.data.player;
    if (player.has<CAnimState>()) {
        const CPhysics& phys = player.get<CPhysics>();
        const CVelocity& vel = player.get<CVelocity>();
        const CAttack& atk = player.get<CAttack>();
        CHealth& hp = player.ensure<CHealth>();
        CAnimState& as = player.ensure<CAnimState>();

        const bool justLanded = !as.wasOnGround && phys.onGround && as.airTime >= kLandMinAirTime;

        if (phys.onGround) {
            as.airTime = 0.f;
        } else {
            as.airTime += dt;
        }

        AssetRegistry& assets = s.worldState.data.vulkan.assets();
        const AnimationClip* landClip = assets.getAnimation("land");
        const float landDuration = landClip ? landClip->duration : 0.3f;
        const AnimationClip* deathClip = assets.getAnimation("death");
        const float deathDuration = deathClip ? deathClip->duration : 1.5f;

        AnimState desired = as.current;

        if (as.current == AnimState::Dead) {
            as.stateTimer += dt;
            if (as.stateTimer >= deathDuration) {
                hp.respawn();
                player.ensure<CTransform>().pos = {0.f, 0.5f, 0.f};
                player.ensure<CVelocity>().y = 0.f;
                std::cout << "[AnimState] Dead complete, respawning\n";
                desired = AnimState::Idle;
            }
        } else if (hp.isDead()) {
            desired = AnimState::Dead;
        } else {
            const AnimState attackState = decideAttackState(atk);
            if (attackState != AnimState::Idle) {
                desired = attackState;
            } else if (as.current == AnimState::Land) {
                as.stateTimer += dt;
                const bool jumpCancel = hasJumpInput(input);
                const bool moveCancel = hasMovementInput(input);
                const bool airborne = !phys.onGround;
                const bool timeUp = as.stateTimer >= landDuration;
                if (jumpCancel || moveCancel || airborne || timeUp) {
                    desired = decideNormalState(phys, vel, input, as.airTime, as.current);
                }
            } else if (isAttackState(as.current)) {
                desired = decideNormalState(phys, vel, input, as.airTime, as.current);
            } else {
                if (justLanded) {
                    desired = AnimState::Land;
                } else {
                    desired = decideNormalState(phys, vel, input, as.airTime, as.current);
                }
            }
        }

        applyTransition(as, desired, "Player");
        as.wasOnGround = phys.onGround;
    }

    // =========================================================================
    // 2. 敵の AnimState 判定 (入力に依存しない)
    // =========================================================================
    s.worldState.data.world.each([&](flecs::entity e, CEnemyAI& ai, CAnimState& as) {
        if (!e.has<EnemyTag>()) return;

        const float velSq =
            ai.moveVelocity.x * ai.moveVelocity.x + ai.moveVelocity.y * ai.moveVelocity.y;
        const AnimState desired = decideEnemyAnimState(ai, velSq);

        const char* label = e.name().c_str();
        if (!label || label[0] == '\0') label = "enemy";
        applyTransition(as, desired, label);
    });
}

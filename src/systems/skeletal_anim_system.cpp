// src/systems/skeletal_anim_system.cpp
// =============================================================================
// Phase 5-A: 敵専用アニメに HitReact / Dead を追加
//
//   敵 HitReact → enemy_hit (なければ Player 用 hit_react、それもなければ idle)
//   敵 Dead    → enemy_death (なければ Player 用 death)
//
// CAttack: 新 API (def->totalDuration()) に対応。
// =============================================================================
#include "systems/skeletal_anim_system.h"

#include "core/attack_def.h"
#include "core/components.h"
#include "core/game_state.h"
#include "renderer/asset_registry.h"

namespace {

float computeAttackPlaySpeed(const AnimationClip* clip, float atkDuration) {
    if (!clip) return 1.f;
    if (clip->duration <= 0.f) return 1.f;
    if (atkDuration <= 0.f) return 1.f;
    return clip->duration / atkDuration;
}

float decideBlendDuration(AnimState from, AnimState to) {
    if (to == AnimState::HitReact) return 0.05f;
    if (from == AnimState::HitReact) return 0.10f;

    if (isAttackState(to)) return 0.05f;
    if (isAttackState(from)) return 0.10f;
    if (to == AnimState::Dead) return 0.f;
    if (to == AnimState::Land) return 0.08f;
    if ((from == AnimState::Run && to == AnimState::Walk) ||
        (from == AnimState::Walk && to == AnimState::Run)) {
        return 0.10f;
    }
    if (to == AnimState::Jump || to == AnimState::Fall) return 0.10f;
    return 0.15f;
}

const char* resolveEnemyAnimAssetName(AnimState state) {
    switch (state) {
        case AnimState::Idle:
            return "enemy_idle";
        case AnimState::Walk:
            return "enemy_walk";
        case AnimState::Slash:
            return "enemy_attack";
        case AnimState::HitReact:
            return "enemy_hit";
        case AnimState::Dead:
            return "enemy_death";
        default:
            return nullptr;
    }
}

const AnimationClip* resolveClipForEntity(AssetRegistry& assets, AnimState state, flecs::entity e) {
    if (e.has<EnemyTag>()) {
        const char* enemyName = resolveEnemyAnimAssetName(state);
        if (enemyName) {
            const AnimationClip* clip = assets.getAnimation(enemyName);
            if (clip) return clip;
        }
    }

    const char* assetName = resolveAnimAssetName(state);
    const AnimationClip* clip = assets.getAnimation(assetName);
    if (clip) return clip;

    if (state == AnimState::SmashDown) {
        clip = assets.getAnimation("slash");
        if (clip) return clip;
    }

    if (state == AnimState::HitReact) {
        clip = assets.getAnimation("idle");
        if (clip) return clip;
    }

    clip = assets.getAnimation("walk");
    if (clip) return clip;
    return assets.getAnimation("idle");
}

}  // namespace

void SkeletalAnimSystem::update(GameState& s, float dt) const {
    AssetRegistry& assets = s.worldState.data.vulkan.assets();

    s.worldState.data.world.each([&](flecs::entity e, CAnimState& as, CSkeletalAnim& sa) {
        const bool needNewClip =
            (as.current != as.previous) || (sa.animator.currentClip() == nullptr);

        if (needNewClip) {
            const AnimationClip* clip = resolveClipForEntity(assets, as.current, e);
            if (clip) {
                const float blend = decideBlendDuration(as.previous, as.current);
                sa.animator.setClip(clip, blend);
            }
        }

        // 攻撃アニメの再生速度: クリップ長 / 攻撃の totalDuration
        // 攻撃 totalDuration は AttackDef から取得 (def が無ければスキップ)
        if (isAttackState(as.current) && e.has<CAttack>()) {
            const CAttack& atk = e.get<CAttack>();
            if (atk.def) {
                const AnimationClip* clip = sa.animator.currentClip();
                sa.speed = computeAttackPlaySpeed(clip, atk.def->totalDuration());
            } else {
                sa.speed = 1.f;
            }
        } else {
            sa.speed = 1.f;
        }
    });

    s.worldState.data.world.each([dt](CSkeletalAnim& sa) {
        if (!sa.playing) return;
        if (!sa.animator.ready()) return;

        sa.animator.update(dt * sa.speed);
        sa.animator.computeSkinMatrices(sa.skinMatrices);
    });
}

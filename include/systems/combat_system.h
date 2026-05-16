#pragma once

#include <flecs.h>

#include "/core/components.h"

class CombatSystem {
public:
    // 横薙ぎ攻撃（F キー）。sweepDeg=150, duration=0.28
    void requestAttack(flecs::entity player) const;

    // 振り下ろし強攻撃（G キー）。sweepDeg=90, duration=0.35
    void requestStrongAttack(flecs::entity player) const;

    // 攻撃タイマー更新。
    void update(flecs::entity player, float dt) const;

    // 入力ロック判定。空中攻撃（isAerial=true）の場合は false を返す（移動制限なし）。
    bool isInputLocked(flecs::entity player) const;

    // 着地時に空中攻撃をキャンセルする。地上攻撃・非攻撃中は何もしない。
    void cancelAerialOnLanding(flecs::entity player) const;

    // 攻撃可視化などで使う現在のスイープ角（ワールドYaw）。
    float getAttackYawDeg(const CTransform& transform, const CAttack& attack) const;
};

// src/systems/combat_system.cpp
#include "systems/combat_system.h"

#include <algorithm>
#include <glm/glm.hpp>

void CombatSystem::requestAttack(flecs::entity player) const {
    auto& atk = player.ensure<CAttack>();
    if (atk.active) return;

    atk.duration = 0.5f;
    atk.sweepDeg = 150.f;
    atk.attackType = AttackType::Slash;
    atk.active = true;
    atk.timer = atk.duration;
    atk.isAerial = !player.get<CPhysics>().onGround;
    atk.isDiving = false;
    atk.diveLiftStartY = 0.f;
    atk.diveLiftHeight = 0.f;
    atk.diveDropStarted = false;
    atk.phaseHitMask = 0;
}

void CombatSystem::requestStrongAttack(flecs::entity player) const {
    auto& atk = player.ensure<CAttack>();
    if (atk.active) return;

    const CPhysics& pp = player.get<CPhysics>();
    const bool inAir = !pp.onGround;

    // 空中の場合はダブルジャンプを消費した後でないと発火しない
    if (inAir && !pp.usedDoubleJump) return;

    // 振り下ろしのパラメータ
    atk.duration = 0.6f;
    atk.sweepDeg = 90.f;
    // Phase 3-E: 地上は Smash、空中は SmashDown と区別 (アニメ用)
    atk.attackType = inAir ? AttackType::SmashDown : AttackType::Smash;
    atk.active = true;
    atk.timer = atk.duration;
    atk.isAerial = inAir;
    atk.isDiving = inAir;  // 空中のみ急降下、地上は通常の振り下ろし
    atk.diveDropStarted = false;
    atk.phaseHitMask = 0;

    if (atk.isDiving) {
        const auto& tr = player.get<CTransform>();
        atk.diveLiftStartY = tr.pos.y;
        atk.diveLiftHeight = tr.scale.y * 1.f;
        player.ensure<CVelocity>().y = 0.f;
    }
}

void CombatSystem::update(flecs::entity player, float dt) const {
    auto& atk = player.ensure<CAttack>();
    if (!atk.active) return;

    atk.timer -= dt;
    if (atk.timer <= 0.f) {
        atk.active = false;
        atk.timer = 0.f;
        atk.isAerial = false;
        atk.isDiving = false;
        atk.diveLiftStartY = 0.f;
        atk.diveLiftHeight = 0.f;
        atk.diveDropStarted = false;
        atk.phaseHitMask = 0;
        atk.attackType = AttackType::Slash;
    }
}

bool CombatSystem::isInputLocked(flecs::entity player) const {
    const auto& atk = player.get<CAttack>();
    if (!atk.active) return false;
    if (atk.isDiving) return true;
    return !atk.isAerial;
}

void CombatSystem::cancelAerialOnLanding(flecs::entity player) const {
    auto& atk = player.ensure<CAttack>();
    if (atk.active && atk.isAerial) {
        atk.active = false;
        atk.timer = 0.f;
        atk.isAerial = false;
        atk.isDiving = false;
        atk.diveLiftStartY = 0.f;
        atk.diveLiftHeight = 0.f;
        atk.diveDropStarted = false;
        atk.phaseHitMask = 0;
        atk.attackType = AttackType::Slash;
    }
}

float CombatSystem::getAttackYawDeg(const CTransform& transform, const CAttack& attack) const {
    const float progress = 1.f - (attack.timer / std::max(attack.duration, 0.0001f));
    const float start = -attack.sweepDeg * 0.5f;
    const float end = attack.sweepDeg * 0.5f;
    const float local = start + (end - start) * std::clamp(progress, 0.f, 1.f);
    return transform.yaw + local;
}

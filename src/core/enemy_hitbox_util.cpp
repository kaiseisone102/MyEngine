// =============================================================================
// core/enemy_hitbox_util.cpp — 敵攻撃 hitbox 計算の実装
// =============================================================================
// 旧 SimulationSystem::makeGroundEnemyPunchHitbox から移植。
// ロジック自体は完全に同じ (判定の挙動を変えないため)。
// =============================================================================
#include "core/enemy_hitbox_util.h"

#include <cmath>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/components.h"

namespace enemy_hitbox {

PunchHitbox makeGroundPunch(const CTransform& et, const CEnemyAI& ai, bool isSkeleton) {
    const float yawRad = glm::radians(et.yaw);
    const glm::vec3 fwd{std::sin(yawRad), 0.f, std::cos(yawRad)};
    const glm::vec3& sc = et.scale;

    glm::vec3 half;
    if (isSkeleton) {
        half.x = sc.x * 0.35f;
        half.y = sc.y * 0.25f;
        half.z = sc.z * 0.35f;
    } else {
        // Soldier
        half.x = sc.x * 0.55f;
        half.y = sc.y * 0.22f;
        half.z = sc.z * 0.5f;
    }

    // 中心は敵の前方 attackRange/2 の位置、 高さは half.y (= 中心が hitbox の真ん中)
    glm::vec3 center = et.pos + fwd * (ai.attackRange * 0.5f);
    center.y = et.pos.y + half.y;

    return {center, half};
}

}  // namespace enemy_hitbox

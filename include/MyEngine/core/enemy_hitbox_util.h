#pragma once
// =============================================================================
// core/enemy_hitbox_util.h — 敵の攻撃 hitbox 共通計算
// =============================================================================
// 役割:
//   Skeleton / Soldier の地上敵がパンチ攻撃する際の hitbox AABB を計算する。
//   判定 (simulation_system) と可視化 (gameplay_layer のデバッグ描画) で
//   共通の計算式を使うため、 ここに切り出した独立 util。
//
// 設計:
//   - 敵の前方ベクトル (yaw) と attackRange、 scale から hitbox を構築
//   - Skeleton と Soldier で half サイズが異なる (Soldier の方が大きい)
//   - 結果は AABB::fromCenterHalf で使える形 (center + half)
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct CTransform;
struct CEnemyAI;

namespace enemy_hitbox {

// パンチ hitbox: 中心 + 半サイズ。
// AABB::fromCenterHalf(box.center, box.half) でそのまま AABB 化可能。
struct PunchHitbox {
    glm::vec3 center;
    glm::vec3 half;
};

// 地上敵 (Skeleton / Soldier) のパンチ hitbox を計算。
//   et         : 敵の Transform (pos = 足元、 scale = 全サイズ)
//   ai         : 敵の AI (attackRange を読む)
//   isSkeleton : Skeleton なら true、 Soldier なら false (half サイズが異なる)
PunchHitbox makeGroundPunch(const CTransform& et, const CEnemyAI& ai, bool isSkeleton);

}  // namespace enemy_hitbox

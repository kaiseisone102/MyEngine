#pragma once
// =============================================================================
// enemy_hitbox_util.h — 敵 hurtbox / attack hitbox の円柱を計算
// =============================================================================

#include <flecs.h>

#include "core/cylinder.h"

namespace enemy_hitbox_util {

// 敵の hurtbox (体の中心円柱) を取得
Cylinder hurtboxOf(flecs::entity e);

// 敵の attack hitbox (前方の円柱、 攻撃モーション中のみ有効)
Cylinder attackHitboxOf(flecs::entity e);

}  // namespace enemy_hitbox_util

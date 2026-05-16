#pragma once

#include <flecs.h>

#include <vector>

#include "core/aabb.h"
#include "core/components.h"

class PhysicsSystem {
   public:
    // プレイヤー用: 重力・ジャンプ・垂直衝突・リスポーン
    // 戻り値: リスポーンしたかどうか
    bool update(flecs::entity player, const std::vector<flecs::entity>& platforms, float dt,
                float gravity, float jumpSpeed) const;

    // 骸骨（敵）用: 重力・垂直衝突のみ（ジャンプなし、リスポーンは呼び出し側で判断）
    // 戻り値: プラットフォームに乗っているか（将来パトロール等に使える）
    bool applyEnemyGravity(flecs::entity enemy, const std::vector<flecs::entity>& platforms,
                           float dt, float gravity) const;

   private:
    static AABB entityAABB(flecs::entity e);

    // プレイヤー専用（CPhysics::onGround を更新する）
    void resolveVerticalCollisions(flecs::entity player,
                                   const std::vector<flecs::entity>& platforms) const;

    // エンティティ汎用（CPhysics 不要、onGround を bool で返す）
    bool resolveVerticalForEntity(flecs::entity entity,
                                  const std::vector<flecs::entity>& platforms) const;
};

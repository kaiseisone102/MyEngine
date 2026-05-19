#pragma once
// =============================================================================
// physics_system.h — + obstacles (CObstacle 持ち) の垂直衝突対応
// =============================================================================
// 垂直衝突: platforms に加えて obstacles も渡せる。
//   両者ともプレイヤーが上に乗れる + 下からぶつかる対象。
//   AABB の取得方法だけ違う (entityAABB vs obstacleWorldAABB)。
// =============================================================================

#include <flecs.h>

#include <vector>

#include "core/aabb.h"
#include "core/components.h"

class WorldTerrain;

class PhysicsSystem {
   public:
    // プレイヤー用: 重力・ジャンプ・垂直衝突・リスポーン
    bool update(flecs::entity player, const std::vector<flecs::entity>& platforms,
                const std::vector<flecs::entity>& obstacles,
                const WorldTerrain* terrain, float dt, float gravity, float jumpSpeed) const;

    // 敵用: 重力・垂直衝突のみ (obstacles はプレイヤーのみ、 敵は無視)
    bool applyEnemyGravity(flecs::entity enemy, const std::vector<flecs::entity>& platforms,
                           const WorldTerrain* terrain, float dt, float gravity) const;

   private:
    static AABB entityAABB(flecs::entity e);

    void resolveVerticalCollisions(flecs::entity player,
                                   const std::vector<flecs::entity>& platforms,
                                   const std::vector<flecs::entity>& obstacles,
                                   const WorldTerrain* terrain) const;

    bool resolveVerticalForEntity(flecs::entity entity,
                                  const std::vector<flecs::entity>& platforms,
                                  const WorldTerrain* terrain) const;
};

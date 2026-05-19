#pragma once
// =============================================================================
// movement_system.h — + resolveObstacleCollisions
// =============================================================================

#include <flecs.h>

#include <vector>

#include "core/aabb.h"
#include "core/camera.h"
#include "core/components.h"

struct ActionState;
class WorldTerrain;

class MovementSystem {
   public:
    void updateTpsPlayerMove(flecs::entity player, const Camera& camera,
                             const ActionState& input, const WorldTerrain* terrain,
                             float dt) const;

    void resolveHorizontalCollisions(flecs::entity entity,
                                     const std::vector<flecs::entity>& platforms) const;

    // 障害物 (CObstacle 持ち = 木/墓/宝箱/岩) との水平衝突。
    // platforms と違って localAABB をモデルから取るため別関数。
    // 水平方向のみ押し出し (Y 衝突なし = 上に乗ったり下からくぐったりはできない)。
    void resolveObstacleCollisions(flecs::entity entity,
                                   const std::vector<flecs::entity>& obstacles) const;

    void resolveEntityVsEntity(flecs::entity a, flecs::entity b) const;

    void moveWithSlide(flecs::entity entity, glm::vec2 velocity,
                       const std::vector<flecs::entity>& platforms) const;
};

#pragma once

#include <flecs.h>

#include <vector>

#include "core/aabb.h"
#include "core/camera.h"
#include "core/components.h"

class MovementSystem {
   public:
    // TPS 移動（攻撃中などロック時は呼び出し側で止める）
    void updateTpsPlayerMove(flecs::entity player, const Camera& camera, const bool* keys,
                             float dt) const;

    // エンティティ vs 足場・壁（XZ）水平衝突解決
    void resolveHorizontalCollisions(flecs::entity entity,
                                     const std::vector<flecs::entity>& platforms) const;

    // エンティティ vs エンティティ（XZ）水平衝突解決
    // 両者を penetration の半分ずつ押し出す（対称応答）
    void resolveEntityVsEntity(flecs::entity a, flecs::entity b) const;

    // 骸骨用スライド移動（X・Z を分割して独立に衝突解決する）
    // ─ X を動かして X だけ解決 -> Z を動かして Z だけ解決 ─
    // この順序により、壁に正面衝突しても Z 方向の速度が残り
    // 壁に沿って自然に滑るようになる。
    // velocity : CEnemyAI::moveVelocity（dt 適用済みの XZ 移動量）
    void moveWithSlide(flecs::entity entity, glm::vec2 velocity,
                       const std::vector<flecs::entity>& platforms) const;
};

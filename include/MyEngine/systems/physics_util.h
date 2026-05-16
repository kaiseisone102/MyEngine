// C:\MyEngine\include\MyEngine\systems\physics_util.h

#pragma once
// =============================================================================
// physics_util.h — 物理共通ヘルパー (Phase 5-F)
// =============================================================================
// 物理判定で繰り返し使うパターンを集約。
// すべての entity は 「pos.y = 足元、 scale = (幅, 高さ, 奥行)」 という
// 共通規約に従っているため、 AABB::fromBottomCenter 1 つで統一できる。
// =============================================================================

#include <flecs.h>

#include "core/aabb.h"
#include "core/components.h"

namespace physics {

// CTransform を持つ任意 entity の AABB を取得。
// すべての entity が足元基準なので fromBottomCenter で統一。
inline AABB entityAABB(flecs::entity e) {
    const auto& t = e.get<CTransform>();
    return AABB::fromBottomCenter(t.pos, t.scale);
}

// pos / scale から直接 AABB を作る (entity を取らずに済む場合用)。
inline AABB makeAABB(const glm::vec3& pos, const glm::vec3& scale) {
    return AABB::fromBottomCenter(pos, scale);
}

}  // namespace physics

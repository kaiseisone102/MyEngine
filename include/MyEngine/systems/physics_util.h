#pragma once
// =============================================================================
// physics_util.h — 物理共通ヘルパー + obstacleWorldAABB
// =============================================================================
// entityAABB:
//   CTransform を持つ entity 全般。 pos/scale/yaw から計算 (足元基準)。
//
// obstacleWorldAABB:
//   CObstacle (= モデルの localAABB を保持) + CTransform からワールド AABB を計算。
//   localAABB は原点中心とは限らない (例: tree_noLeaves_2 は min=(0,0,0))、
//   なので「足元基準スケール」 ではなく、 localAABB.min/max を scale で乗算し
//   yaw 回転、 pos 平行移動する。
// =============================================================================

#include <flecs.h>

#include <algorithm>
#include <cmath>

#include "core/aabb.h"
#include "core/components.h"
#include "core/cylinder.h"
#include "core/obstacle.h"

namespace physics {

inline AABB entityAABB(flecs::entity e) {
    const auto& t = e.get<CTransform>();
    return AABB::fromBottomCenterYawed(t.pos, t.scale, t.yaw);
}

inline AABB makeAABB(const glm::vec3& pos, const glm::vec3& scale) {
    return AABB::fromBottomCenter(pos, scale);
}

inline Cylinder entityCylinder(flecs::entity e) {
    const auto& t = e.get<CTransform>();
    return Cylinder::fromBottomCenter(t.pos, t.scale);
}

inline Cylinder makeCylinder(const glm::vec3& pos, const glm::vec3& scale) {
    return Cylinder::fromBottomCenter(pos, scale);
}

// CObstacle (モデルの localAABB) + CTransform から、 ワールド空間の AABB を計算。
// 手順:
//   1. localAABB を scale で乗算 (= モデルサイズ反映)
//   2. yaw 回転 (XZ 平面、 8 頂点 → 軸並行 AABB)
//   3. pos で平行移動
//
// yaw=0 のとき: 単純な scale + translate になる。
inline AABB obstacleWorldAABB(flecs::entity e) {
    const CTransform& t = e.get<CTransform>();
    const CObstacle& o = e.get<CObstacle>();
    const AABB& lab = o.localAABB;

    // scaled local AABB (まだ pos なし、 まだ yaw なし)
    const glm::vec3 mnL{lab.min.x * t.scale.x, lab.min.y * t.scale.y, lab.min.z * t.scale.z};
    const glm::vec3 mxL{lab.max.x * t.scale.x, lab.max.y * t.scale.y, lab.max.z * t.scale.z};

    // Y はそのまま (yaw 回転は XZ のみ)
    const float minY = std::min(mnL.y, mxL.y);
    const float maxY = std::max(mnL.y, mxL.y);

    // XZ 4 隅を yaw 回転
    const float r = t.yaw * 0.017453293f;
    const float c = std::cos(r);
    const float s = std::sin(r);

    const float xs[4] = {mnL.x, mnL.x, mxL.x, mxL.x};
    const float zs[4] = {mnL.z, mxL.z, mnL.z, mxL.z};

    float minX = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float minZ = std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();

    for (int i = 0; i < 4; ++i) {
        // yaw 回転: x' = x*c + z*s, z' = -x*s + z*c (CTransform::matrix と同じ慣習)
        const float xr = xs[i] * c + zs[i] * s;
        const float zr = -xs[i] * s + zs[i] * c;
        minX = std::min(minX, xr);
        maxX = std::max(maxX, xr);
        minZ = std::min(minZ, zr);
        maxZ = std::max(maxZ, zr);
    }

    return AABB::fromMinMax(
        {t.pos.x + minX, t.pos.y + minY, t.pos.z + minZ},
        {t.pos.x + maxX, t.pos.y + maxY, t.pos.z + maxZ});
}

}  // namespace physics

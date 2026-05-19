#pragma once
// =============================================================================
// cylinder.h — 円柱型 + 判定ヘルパー (Phase 2)
// =============================================================================
// Phase 1: 描画用
// Phase 2: 食らい判定にも使う (overlap 系関数を追加)
//
// 判定ヘルパー:
//   overlap(Cylinder, Cylinder) : 円柱同士 (XZ 距離 + Y 範囲)
//   overlap(Cylinder, AABB)     : 円柱と AABB (XZ 円 vs 矩形 + Y 範囲)
//
// Phase 3 で plat/terrain 衝突解決にも使う。
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#include "core/aabb.h"

struct Cylinder {
    glm::vec3 baseCenter{0.f};
    float     radius = 0.f;
    float     height = 0.f;

    // ─── ファクトリ ─────────────────────────────────────
    static Cylinder fromBottomCenter(const glm::vec3& pos, const glm::vec3& scale) {
        return Cylinder{
            pos,
            (scale.x + scale.z) * 0.25f,
            scale.y
        };
    }

    static Cylinder fromAABB(const AABB& box) {
        const glm::vec3 h = box.half();
        return Cylinder{
            glm::vec3{(box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f},
            (h.x + h.z) * 0.5f,
            box.max.y - box.min.y
        };
    }

    // ─── アクセサ ─────────────────────────────────────
    float bottomY() const { return baseCenter.y; }
    float topY()    const { return baseCenter.y + height; }
    float midY()    const { return baseCenter.y + height * 0.5f; }

    glm::vec3 bottomCenter() const { return baseCenter; }
    glm::vec3 topCenter()    const { return {baseCenter.x, topY(), baseCenter.z}; }
    glm::vec3 midCenter()    const { return {baseCenter.x, midY(), baseCenter.z}; }

    // XZ 平面の中心 (Y は無視) と XZ 距離計算用ヘルパー
    glm::vec2 centerXZ() const { return {baseCenter.x, baseCenter.z}; }
};

namespace cylinder {

// 円柱同士の重なり判定。
//   XZ 平面: 中心間距離 <= 半径の和
//   Y 方向: 範囲 [bottomY..topY] が重なる
inline bool overlap(const Cylinder& a, const Cylinder& b) {
    const float dx = a.baseCenter.x - b.baseCenter.x;
    const float dz = a.baseCenter.z - b.baseCenter.z;
    const float rSum = a.radius + b.radius;
    if (dx * dx + dz * dz > rSum * rSum) return false;

    if (a.topY() <= b.bottomY()) return false;
    if (b.topY() <= a.bottomY()) return false;
    return true;
}

// 円柱と AABB の重なり判定。
//   XZ 平面: 円が AABB の XZ 矩形に最も近い点との距離 <= radius
//   Y 方向: 範囲が重なる
inline bool overlap(const Cylinder& c, const AABB& box) {
    // Y 範囲チェック
    if (c.topY() <= box.min.y) return false;
    if (box.max.y <= c.bottomY()) return false;

    // XZ 平面で、 円中心から最も近い AABB の点を計算 (clamp で求める)
    const float cx = c.baseCenter.x;
    const float cz = c.baseCenter.z;
    const float nx = std::max(box.min.x, std::min(cx, box.max.x));
    const float nz = std::max(box.min.z, std::min(cz, box.max.z));
    const float dx = cx - nx;
    const float dz = cz - nz;
    return (dx * dx + dz * dz) <= (c.radius * c.radius);
}

// 円柱の中心 (XZ) と、 点 (XZ) との距離の 2 乗。
// performSweepHit などで「攻撃 origin から敵円柱中心まで」 を計算するのに使う。
inline float distanceXZSq(const Cylinder& c, const glm::vec3& p) {
    const float dx = c.baseCenter.x - p.x;
    const float dz = c.baseCenter.z - p.z;
    return dx * dx + dz * dz;
}

// 円柱の Y 範囲が指定 Y を含むか
inline bool containsY(const Cylinder& c, float y) {
    return y >= c.bottomY() && y <= c.topY();
}

// 円柱の Y 範囲と [yMin..yMax] が重なるか
inline bool overlapsYRange(const Cylinder& c, float yMin, float yMax) {
    return c.topY() > yMin && c.bottomY() < yMax;
}

}  // namespace cylinder

#pragma once
// =============================================================================
// aabb.h — AABB 衝突判定 (Phase 5-F + yaw 対応)
// =============================================================================
// 追加: fromBottomCenterYawed - yaw 回転後の頂点を包む軸並行 AABB を生成。
//   yaw=0 のとき fromBottomCenter と完全に等価 (sin=0, cos=1)。
//   開き戸 (CGate::Rotate) のような回転 entity の判定に使う。
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

struct AABBHit {
    enum class Axis { None, X, Y, Z };

    bool      overlap = false;
    Axis      axis    = Axis::None;
    float     depth   = 0.f;
    glm::vec3 normal  = {0.f, 0.f, 0.f};

    glm::vec3 displacement() const { return normal * depth; }
    bool isFromTop() const    { return axis == Axis::Y && normal.y > 0.f; }
    bool isFromBottom() const { return axis == Axis::Y && normal.y < 0.f; }
    bool isHorizontal() const { return axis == Axis::X || axis == Axis::Z; }
};


struct AABB {
    glm::vec3 min{};
    glm::vec3 max{};

    static AABB fromMinMax(const glm::vec3& min, const glm::vec3& max) {
        return AABB{min, max};
    }

    static AABB fromCenterHalf(const glm::vec3& center, const glm::vec3& half) {
        return AABB{center - half, center + half};
    }

    // 足元中心 + 全サイズ。 yaw 無視 (= 軸並行のまま)。
    static AABB fromBottomCenter(const glm::vec3& bottomCenter, const glm::vec3& scale) {
        const glm::vec3 half = scale * 0.5f;
        return AABB{
            {bottomCenter.x - half.x, bottomCenter.y,            bottomCenter.z - half.z},
            {bottomCenter.x + half.x, bottomCenter.y + scale.y,  bottomCenter.z + half.z}
        };
    }

    // 足元中心 + 全サイズ + yaw (deg)。
    // XZ 平面で 4 隅を yaw 回転させ、 それを包む軸並行 AABB を返す。
    // yaw=0 のとき fromBottomCenter と完全一致。
    //
    // yaw は CTransform::yaw と同じ慣習 (Y 軸回り、 deg)。
    // 注意: 回転後の AABB は元の AABB より大きくなる傾向がある (= 45° で √2 倍幅)。
    //       これは「回転を考慮した軸並行 box」 の性質上避けられない。
    //       開き戸 (75° 等) では「板の長辺方向に bbox が広がる」 形になる。
    static AABB fromBottomCenterYawed(const glm::vec3& bottomCenter, const glm::vec3& scale,
                                       float yawDeg) {
        const float r = yawDeg * 0.017453293f;  // deg → rad
        const float c = std::cos(r);
        const float s = std::sin(r);

        const float hx = scale.x * 0.5f;
        const float hz = scale.z * 0.5f;

        // XZ 平面の 4 隅 (local: 板中心基準)
        // (+hx, +hz), (+hx, -hz), (-hx, +hz), (-hx, -hz) を yaw 回転
        // yaw 回転: x' = x*c + z*s, z' = -x*s + z*c
        // (yaw=0 で恒等変換になるよう CTransform::matrix と同一の慣習)
        const float cornersX[4] = {
            ( hx) * c + ( hz) * s,
            ( hx) * c + (-hz) * s,
            (-hx) * c + ( hz) * s,
            (-hx) * c + (-hz) * s,
        };
        const float cornersZ[4] = {
            -( hx) * s + ( hz) * c,
            -( hx) * s + (-hz) * c,
            -(-hx) * s + ( hz) * c,
            -(-hx) * s + (-hz) * c,
        };

        float minX = cornersX[0], maxX = cornersX[0];
        float minZ = cornersZ[0], maxZ = cornersZ[0];
        for (int i = 1; i < 4; ++i) {
            minX = std::min(minX, cornersX[i]);
            maxX = std::max(maxX, cornersX[i]);
            minZ = std::min(minZ, cornersZ[i]);
            maxZ = std::max(maxZ, cornersZ[i]);
        }

        return AABB{
            {bottomCenter.x + minX, bottomCenter.y,            bottomCenter.z + minZ},
            {bottomCenter.x + maxX, bottomCenter.y + scale.y,  bottomCenter.z + maxZ}
        };
    }

    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 size()   const { return max - min; }
    glm::vec3 half()   const { return (max - min) * 0.5f; }

    float topY()    const { return max.y; }
    float bottomY() const { return min.y; }
    float leftX()   const { return min.x; }
    float rightX()  const { return max.x; }
    float frontZ()  const { return min.z; }
    float backZ()   const { return max.z; }

    bool isEmpty() const {
        return max.x <= min.x || max.y <= min.y || max.z <= min.z;
    }

    bool contains(const glm::vec3& p) const {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    bool overlaps(const AABB& o) const {
        return min.x < o.max.x && max.x > o.min.x
            && min.y < o.max.y && max.y > o.min.y
            && min.z < o.max.z && max.z > o.min.z;
    }

    AABB expanded(float margin) const {
        const glm::vec3 m{margin};
        return AABB{min - m, max + m};
    }

    AABB translated(const glm::vec3& d) const {
        return AABB{min + d, max + d};
    }

    AABBHit sweep(const AABB& o) const {
        AABBHit hit{};
        if (!overlaps(o)) return hit;
        hit.overlap = true;

        const float penX = std::min(max.x, o.max.x) - std::max(min.x, o.min.x);
        const float penY = std::min(max.y, o.max.y) - std::max(min.y, o.min.y);
        const float penZ = std::min(max.z, o.max.z) - std::max(min.z, o.min.z);

        const glm::vec3 myC = center();
        const glm::vec3 oC  = o.center();

        if (penX <= penY && penX <= penZ) {
            hit.axis   = AABBHit::Axis::X;
            hit.depth  = penX;
            hit.normal = {(myC.x >= oC.x) ? 1.f : -1.f, 0.f, 0.f};
        } else if (penY <= penZ) {
            hit.axis   = AABBHit::Axis::Y;
            hit.depth  = penY;
            hit.normal = {0.f, (myC.y >= oC.y) ? 1.f : -1.f, 0.f};
        } else {
            hit.axis   = AABBHit::Axis::Z;
            hit.depth  = penZ;
            hit.normal = {0.f, 0.f, (myC.z >= oC.z) ? 1.f : -1.f};
        }
        return hit;
    }

    AABBHit sweepAxis(const AABB& o, AABBHit::Axis axis) const {
        AABBHit hit{};
        if (!overlaps(o)) return hit;
        hit.overlap = true;
        hit.axis    = axis;

        const glm::vec3 myC = center();
        const glm::vec3 oC  = o.center();

        switch (axis) {
            case AABBHit::Axis::X: {
                hit.depth  = std::min(max.x, o.max.x) - std::max(min.x, o.min.x);
                hit.normal = {(myC.x >= oC.x) ? 1.f : -1.f, 0.f, 0.f};
                break;
            }
            case AABBHit::Axis::Y: {
                hit.depth  = std::min(max.y, o.max.y) - std::max(min.y, o.min.y);
                hit.normal = {0.f, (myC.y >= oC.y) ? 1.f : -1.f, 0.f};
                break;
            }
            case AABBHit::Axis::Z: {
                hit.depth  = std::min(max.z, o.max.z) - std::max(min.z, o.min.z);
                hit.normal = {0.f, 0.f, (myC.z >= oC.z) ? 1.f : -1.f};
                break;
            }
            case AABBHit::Axis::None:
                return sweep(o);
        }
        return hit;
    }

    glm::vec3 penetration(const AABB& o) const {
        return {
            std::min(max.x, o.max.x) - std::max(min.x, o.min.x),
            std::min(max.y, o.max.y) - std::max(min.y, o.min.y),
            std::min(max.z, o.max.z) - std::max(min.z, o.min.z)
        };
    }
};

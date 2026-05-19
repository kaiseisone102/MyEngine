// =============================================================================
// moving_platform_system.cpp — 5 パターン対応
// =============================================================================
#define NOMINMAX
#include "systems/moving_platform_system.h"

#include <cmath>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/components.h"
#include "core/game_state.h"

namespace {

// 振り子の最大振れ角 (ラジアン)。 ±60 度。
constexpr float kPendulumMaxAngle = 1.0472f;  // 60 deg

glm::vec3 normalizeOrZero(const glm::vec3& v) {
    const float lenSq = glm::dot(v, v);
    if (lenSq < 1e-8f) return glm::vec3{0.f};
    return v / std::sqrt(lenSq);
}

// 水平軸の正規化 (Y 成分は捨てる)
glm::vec3 normalizeHorizontal(const glm::vec3& v) {
    glm::vec3 h{v.x, 0.f, v.z};
    return normalizeOrZero(h);
}

glm::vec3 computeNewPos(const CMovingPlatform& mp) {
    switch (mp.pattern) {
        case CMovingPlatform::Pattern::PingPongLinear: {
            const glm::vec3 axisN = normalizeOrZero(mp.axis);
            if (glm::dot(axisN, axisN) < 1e-8f) return mp.originPos;
            return mp.originPos + axisN * (mp.amplitude * std::sin(mp.phase));
        }
        case CMovingPlatform::Pattern::Vertical: {
            return mp.originPos + glm::vec3{0.f, mp.amplitude * std::sin(mp.phase), 0.f};
        }
        case CMovingPlatform::Pattern::Circular: {
            return mp.originPos +
                    glm::vec3{mp.amplitude * std::cos(mp.phase),
                              0.f,
                              mp.amplitude * std::sin(mp.phase)};
        }
        case CMovingPlatform::Pattern::Pendulum: {
            // 支点 origin、 振り子の長さ amplitude、 揺れる方向 axis (水平)
            // 角度 = maxAngle * sin(phase) (= 単振動)
            const glm::vec3 axisH = normalizeHorizontal(mp.axis);
            if (glm::dot(axisH, axisH) < 1e-8f) {
                return mp.originPos - glm::vec3{0.f, mp.amplitude, 0.f};
            }
            const float angle = kPendulumMaxAngle * std::sin(mp.phase);
            const float horizOffset = mp.amplitude * std::sin(angle);
            const float vertDrop = mp.amplitude * std::cos(angle);
            return mp.originPos + axisH * horizOffset - glm::vec3{0.f, vertDrop, 0.f};
        }
        case CMovingPlatform::Pattern::OrbitVertical: {
            // 縦面の円運動。 axisH (水平成分) と Y で構成される縦面で円運動。
            // pos = origin + axisH * (amplitude * cos(phase)) + Y * (amplitude * sin(phase))
            const glm::vec3 axisH = normalizeHorizontal(mp.axis);
            if (glm::dot(axisH, axisH) < 1e-8f) {
                return mp.originPos + glm::vec3{0.f, mp.amplitude * std::sin(mp.phase), 0.f};
            }
            return mp.originPos +
                    axisH * (mp.amplitude * std::cos(mp.phase)) +
                    glm::vec3{0.f, mp.amplitude * std::sin(mp.phase), 0.f};
        }
    }
    return mp.originPos;
}

}  // namespace

void MovingPlatformSystem::update(WorldData& wd, float dt) const {
    if (dt <= 0.f) return;

    wd.world.each([&](flecs::entity, CTransform& t, CMovingPlatform& mp) {
        const glm::vec3 oldPos = t.pos;

        mp.phase += mp.angularSpeed * dt;
        t.pos = computeNewPos(mp);

        mp.velocity = (t.pos - oldPos) / dt;
    });

    flecs::entity player = wd.player;
    if (!player || !player.is_alive()) return;
    if (!player.has<CPhysics>()) return;

    CPhysics& pp = player.ensure<CPhysics>();
    if (!pp.standingOn || !pp.standingOn.is_alive()) return;
    if (!pp.standingOn.has<CMovingPlatform>()) return;

    const CMovingPlatform& mp = pp.standingOn.get<CMovingPlatform>();

    CTransform& pt = player.ensure<CTransform>();
    pt.pos += mp.velocity * dt;

    CVelocity& pv = player.ensure<CVelocity>();
    pv.xz = glm::vec2{mp.velocity.x, mp.velocity.z};
}

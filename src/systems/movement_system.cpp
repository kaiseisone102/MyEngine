// =============================================================================
// movement_system.cpp — + resolveObstacleCollisions (obstacle 衝突)
// =============================================================================
#include <systems/movement_system.h>

#include <algorithm>
#include <cmath>

#include "core/action_state.h"
#include "core/cylinder.h"
#include "renderer/terrain_mesh.h"
#include "systems/physics_util.h"
#include "world/world_terrain.h"

namespace {

constexpr float kRunMultiplier = 1.7f;
constexpr float kHorizontalCollisionMinYOverlap = 0.05f;
constexpr float kMinWalkableY = 0.7071f;

struct CylAABBHorizHit {
    bool overlap = false;
    glm::vec2 pushDir{0.f};
    float depth = 0.f;
};

CylAABBHorizHit pushOutFromInside(const Cylinder& cyl, const AABB& box) {
    const float cx = cyl.baseCenter.x;
    const float cz = cyl.baseCenter.z;
    const float dLeft  = cx - box.min.x;
    const float dRight = box.max.x - cx;
    const float dFront = cz - box.min.z;
    const float dBack  = box.max.z - cz;

    float minDist = dLeft;
    glm::vec2 dir{-1.f, 0.f};

    if (dRight < minDist) { minDist = dRight; dir = {1.f, 0.f}; }
    if (dFront < minDist) { minDist = dFront; dir = {0.f, -1.f}; }
    if (dBack  < minDist) { minDist = dBack;  dir = {0.f, 1.f}; }

    CylAABBHorizHit hit;
    hit.overlap = true;
    hit.pushDir = dir;
    hit.depth = minDist + cyl.radius;
    return hit;
}

CylAABBHorizHit resolveCylinderAABBHorizontal(const Cylinder& cyl, const AABB& box) {
    CylAABBHorizHit hit{};

    const float yOverlap = std::min(cyl.topY(), box.max.y) - std::max(cyl.bottomY(), box.min.y);
    if (yOverlap < kHorizontalCollisionMinYOverlap) return hit;

    const float cx = cyl.baseCenter.x;
    const float cz = cyl.baseCenter.z;
    const float nx = std::max(box.min.x, std::min(cx, box.max.x));
    const float nz = std::max(box.min.z, std::min(cz, box.max.z));
    const float dx = cx - nx;
    const float dz = cz - nz;
    const float distSq = dx * dx + dz * dz;
    const float r = cyl.radius;

    if (distSq >= r * r) return hit;

    if (distSq < 1e-8f) {
        return pushOutFromInside(cyl, box);
    }

    const float dist = std::sqrt(distSq);
    hit.overlap = true;
    hit.pushDir = glm::vec2(dx / dist, dz / dist);
    hit.depth = r - dist;
    return hit;
}

void resolveOneHorizontal(flecs::entity entity, const AABB& platBox) {
    auto& pt = entity.ensure<CTransform>();
    const Cylinder cyl = physics::entityCylinder(entity);
    const CylAABBHorizHit hit = resolveCylinderAABBHorizontal(cyl, platBox);
    if (!hit.overlap) return;
    pt.pos.x += hit.pushDir.x * hit.depth;
    pt.pos.z += hit.pushDir.y * hit.depth;
}

}  // namespace

void MovementSystem::updateTpsPlayerMove(flecs::entity player, const Camera& camera,
                                         const ActionState& input,
                                         const WorldTerrain* terrain, float dt) const {
    if (camera.mode != CameraMode::TPS) return;

    auto& pt = player.ensure<CTransform>();
    const auto& pp = player.get<CPhysics>();

    bool guardingNow = false;
    if (input.guardHeld && player.has<CShield>()) {
        const CShield& sh = player.get<CShield>();
        if (sh.canGuard()) guardingNow = true;
    }

    glm::vec3 move{0.f};
    move += camera.getTpsForward() * input.moveZ;
    move += camera.getTpsRight()   * input.moveX;

    if (glm::length(move) > 0.001f) {
        const bool runModifier = input.sprint;
        const float speedMult = (runModifier && pp.onGround) ? kRunMultiplier : 1.f;

        move = glm::normalize(move) * pp.speed * speedMult * dt;

        const glm::vec3 inputDir = move;

        if (terrain && pp.onGround) {
            const glm::vec3 nextPos = pt.pos + move;
            const glm::vec3 n = terrain->sampleNormal(nextPos.x, nextPos.z);
            if (n.y < kMinWalkableY) {
                glm::vec3 horizN(n.x, 0.f, n.z);
                const float horizLen = glm::length(horizN);
                if (horizLen > 1e-4f) {
                    horizN /= horizLen;
                    const float downComponent = glm::dot(move, horizN);
                    if (downComponent < 0.f) {
                        move -= horizN * downComponent;
                    }
                }
            }
        }

        if (!guardingNow) {
            pt.pos += move;
        }
        if (pp.onGround) {
            pt.yaw = glm::degrees(std::atan2(inputDir.x, inputDir.z));
        }
    }
}

void MovementSystem::resolveHorizontalCollisions(
    flecs::entity entity, const std::vector<flecs::entity>& platforms) const {
    for (flecs::entity plat : platforms) {
        const AABB platBox = physics::entityAABB(plat);
        resolveOneHorizontal(entity, platBox);
    }
}

void MovementSystem::resolveObstacleCollisions(
    flecs::entity entity, const std::vector<flecs::entity>& obstacles) const {
    for (flecs::entity obs : obstacles) {
        if (!obs.is_alive()) continue;
        if (!obs.has<CObstacle>() || !obs.has<CTransform>()) continue;
        const AABB obsBox = physics::obstacleWorldAABB(obs);
        resolveOneHorizontal(entity, obsBox);
    }
}

void MovementSystem::moveWithSlide(flecs::entity entity, glm::vec2 velocity,
                                   const std::vector<flecs::entity>& platforms) const {
    auto& pt = entity.ensure<CTransform>();

    pt.pos.x += velocity.x;
    for (flecs::entity plat : platforms) {
        const AABB platBox = physics::entityAABB(plat);
        resolveOneHorizontal(entity, platBox);
    }

    pt.pos.z += velocity.y;
    for (flecs::entity plat : platforms) {
        const AABB platBox = physics::entityAABB(plat);
        resolveOneHorizontal(entity, platBox);
    }
}

void MovementSystem::resolveEntityVsEntity(flecs::entity a, flecs::entity b) const {
    auto& ta = a.ensure<CTransform>();
    auto& tb = b.ensure<CTransform>();

    const Cylinder ca = physics::entityCylinder(a);
    const Cylinder cb = physics::entityCylinder(b);

    const float dx = ta.pos.x - tb.pos.x;
    const float dz = ta.pos.z - tb.pos.z;
    const float distSq = dx * dx + dz * dz;
    const float rSum = ca.radius + cb.radius;
    if (distSq >= rSum * rSum) return;

    if (ca.topY() <= cb.bottomY()) return;
    if (cb.topY() <= ca.bottomY()) return;

    if (distSq < 1e-8f) {
        const float halfPush = rSum * 0.5f;
        ta.pos.x += halfPush;
        tb.pos.x -= halfPush;
        return;
    }

    const float dist = std::sqrt(distSq);
    const float penDepth = rSum - dist;
    const float pushX = dx / dist;
    const float pushZ = dz / dist;
    const float halfPush = penDepth * 0.5f;

    ta.pos.x += pushX * halfPush;
    ta.pos.z += pushZ * halfPush;
    tb.pos.x -= pushX * halfPush;
    tb.pos.z -= pushZ * halfPush;
}

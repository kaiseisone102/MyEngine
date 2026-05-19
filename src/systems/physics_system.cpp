// =============================================================================
// physics_system.cpp — + obstacles の垂直衝突
// =============================================================================
// 障害物 (CObstacle 持ち = 木/墓/宝箱/岩) もプレイヤーが「上に乗れる」 ように、
// 垂直衝突を追加。 AABB の取得は physics::obstacleWorldAABB を使う。
// =============================================================================
#include "systems/physics_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/cylinder.h"
#include "core/obstacle.h"
#include "renderer/terrain_mesh.h"
#include "systems/physics_util.h"
#include "world/world_terrain.h"

namespace {

bool resolveTerrainGround(CTransform& t, CVelocity& v, const WorldTerrain* terrain) {
    if (!terrain) return false;
    const float th = terrain->sampleHeight(t.pos.x, t.pos.z);
    if (th == std::numeric_limits<float>::lowest()) return false;
    if (t.pos.y < th) {
        t.pos.y = th;
        if (v.y < 0.f) v.y = 0.f;
        return true;
    }
    return false;
}

bool circleAABBOverlapXZ(float cx, float cz, float r, const AABB& box) {
    const float nx = std::max(box.min.x, std::min(cx, box.max.x));
    const float nz = std::max(box.min.z, std::min(cz, box.max.z));
    const float dx = cx - nx;
    const float dz = cz - nz;
    return (dx * dx + dz * dz) <= (r * r);
}

bool circleCenterInsideAABBXZ(float cx, float cz, const AABB& box) {
    return cx >= box.min.x && cx <= box.max.x &&
            cz >= box.min.z && cz <= box.max.z;
}

struct CylAABBYHit {
    bool overlap = false;
    float depth = 0.f;
    float normalY = 0.f;
};

CylAABBYHit resolveCylinderAABBY(const Cylinder& cyl, const AABB& box) {
    CylAABBYHit hit{};

    if (!circleCenterInsideAABBXZ(cyl.baseCenter.x, cyl.baseCenter.z, box)) {
        return hit;
    }

    const float cyTop = cyl.topY();
    const float cyBot = cyl.bottomY();
    if (cyTop <= box.min.y) return hit;
    if (box.max.y <= cyBot) return hit;

    hit.overlap = true;

    const float cyMidY = cyl.midY();
    const float boxMidY = (box.min.y + box.max.y) * 0.5f;
    const float penY = std::min(cyTop, box.max.y) - std::max(cyBot, box.min.y);

    hit.depth = penY;
    hit.normalY = (cyMidY >= boxMidY) ? 1.f : -1.f;
    return hit;
}

// 床候補 Y を集める (platforms + obstacles + terrain)
float findGroundY(const CTransform& t, const std::vector<flecs::entity>& platforms,
                   const std::vector<flecs::entity>& obstacles,
                   const WorldTerrain* terrain) {
    float bestY = std::numeric_limits<float>::lowest();

    if (terrain) {
        const float th = terrain->sampleHeight(t.pos.x, t.pos.z);
        if (th != std::numeric_limits<float>::lowest()) {
            bestY = std::max(bestY, th);
        }
    }

    const float circleX = t.pos.x;
    const float circleZ = t.pos.z;
    const float radius = (t.scale.x + t.scale.z) * 0.25f;

    for (flecs::entity plat : platforms) {
        if (!plat.is_alive()) continue;
        const AABB platBox = physics::entityAABB(plat);
        if (!circleAABBOverlapXZ(circleX, circleZ, radius, platBox)) continue;
        bestY = std::max(bestY, platBox.max.y);
    }

    for (flecs::entity obs : obstacles) {
        if (!obs.is_alive()) continue;
        if (!obs.has<CObstacle>() || !obs.has<CTransform>()) continue;
        const AABB obsBox = physics::obstacleWorldAABB(obs);
        if (!circleAABBOverlapXZ(circleX, circleZ, radius, obsBox)) continue;
        bestY = std::max(bestY, obsBox.max.y);
    }

    return bestY;
}

bool trySnapToGround(CTransform& t, CVelocity& v, bool wasOnGround, bool jumpingUp,
                     bool jumpReq, const std::vector<flecs::entity>& platforms,
                     const std::vector<flecs::entity>& obstacles,
                     const WorldTerrain* terrain) {
    constexpr float kSnapDistance = 0.5f;
    constexpr float kMinDownwardSpeed = -0.1f;

    if (!wasOnGround || jumpingUp || jumpReq) return false;
    if (v.y >= kMinDownwardSpeed) return false;

    const float groundY = findGroundY(t, platforms, obstacles, terrain);
    if (groundY == std::numeric_limits<float>::lowest()) return false;

    const float heightAbove = t.pos.y - groundY;
    if (heightAbove < 0.f || heightAbove >= kSnapDistance) return false;

    t.pos.y = groundY;
    if (v.y < 0.f) v.y = 0.f;
    return true;
}

}  // namespace

AABB PhysicsSystem::entityAABB(flecs::entity e) { return physics::entityAABB(e); }

void PhysicsSystem::resolveVerticalCollisions(flecs::entity player,
                                              const std::vector<flecs::entity>& platforms,
                                              const std::vector<flecs::entity>& obstacles,
                                              const WorldTerrain* terrain) const {
    auto& pt = player.ensure<CTransform>();
    auto& pv = player.ensure<CVelocity>();
    auto& pp = player.ensure<CPhysics>();
    pp.onGround = false;
    pp.standingOn = flecs::entity::null();

    // Platforms (cube + moving)
    for (flecs::entity plat : platforms) {
        const AABB platBox = physics::entityAABB(plat);
        const Cylinder playerCyl = physics::entityCylinder(player);

        const CylAABBYHit hit = resolveCylinderAABBY(playerCyl, platBox);
        if (!hit.overlap) continue;

        pt.pos.y += hit.normalY * hit.depth;

        if (hit.normalY > 0.f) {
            if (pv.y < 0.f) pv.y = 0.f;
            pp.jumpsRemaining = pp.maxJumps;
            pp.usedDoubleJump = false;
            pp.onGround = true;
            pp.standingOn = plat;
        } else {
            if (pv.y > 0.f) pv.y = 0.f;
        }
    }

    // Obstacles (木/墓/宝箱/岩)
    for (flecs::entity obs : obstacles) {
        if (!obs.is_alive()) continue;
        if (!obs.has<CObstacle>() || !obs.has<CTransform>()) continue;
        const AABB obsBox = physics::obstacleWorldAABB(obs);
        const Cylinder playerCyl = physics::entityCylinder(player);

        const CylAABBYHit hit = resolveCylinderAABBY(playerCyl, obsBox);
        if (!hit.overlap) continue;

        pt.pos.y += hit.normalY * hit.depth;

        if (hit.normalY > 0.f) {
            if (pv.y < 0.f) pv.y = 0.f;
            pp.jumpsRemaining = pp.maxJumps;
            pp.usedDoubleJump = false;
            pp.onGround = true;
            pp.standingOn = obs;
        } else {
            if (pv.y > 0.f) pv.y = 0.f;
        }
    }

    if (resolveTerrainGround(pt, pv, terrain)) {
        pp.jumpsRemaining = pp.maxJumps;
        pp.usedDoubleJump = false;
        pp.onGround = true;
    }
}

bool PhysicsSystem::resolveVerticalForEntity(flecs::entity entity,
                                             const std::vector<flecs::entity>& platforms,
                                             const WorldTerrain* terrain) const {
    auto& et = entity.ensure<CTransform>();
    auto& ev = entity.ensure<CVelocity>();
    bool onGround = false;

    for (flecs::entity plat : platforms) {
        const AABB platBox = physics::entityAABB(plat);
        const Cylinder ecyl = physics::entityCylinder(entity);

        const CylAABBYHit hit = resolveCylinderAABBY(ecyl, platBox);
        if (!hit.overlap) continue;

        et.pos.y += hit.normalY * hit.depth;

        if (hit.normalY > 0.f) {
            if (ev.y < 0.f) ev.y = 0.f;
            onGround = true;
        } else {
            if (ev.y > 0.f) ev.y = 0.f;
        }
    }

    if (resolveTerrainGround(et, ev, terrain)) {
        onGround = true;
    }
    return onGround;
}

bool PhysicsSystem::applyEnemyGravity(flecs::entity enemy,
                                      const std::vector<flecs::entity>& platforms,
                                      const WorldTerrain* terrain, float dt, float gravity) const {
    auto& et = enemy.ensure<CTransform>();
    auto& ev = enemy.ensure<CVelocity>();

    const float g = (ev.y < 0.f) ? gravity * 1.5f : gravity;
    ev.y += g * dt;
    if (ev.y < -40.f) ev.y = -40.f;

    const bool wasOnGround = (ev.y - g * dt) >= -0.05f && (ev.y - g * dt) <= 0.05f;

    et.pos.y += ev.y * dt;
    bool onGround = resolveVerticalForEntity(enemy, platforms, terrain);

    if (wasOnGround && !onGround) {
        // 敵は障害物を無視するので空 obstacles を渡す
        static const std::vector<flecs::entity> kEmpty;
        if (trySnapToGround(et, ev, wasOnGround, /*jumpingUp*/ false, /*jumpReq*/ false,
                              platforms, kEmpty, terrain)) {
            onGround = true;
        }
    }

    return onGround;
}

bool PhysicsSystem::update(flecs::entity player, const std::vector<flecs::entity>& platforms,
                           const std::vector<flecs::entity>& obstacles,
                           const WorldTerrain* terrain, float dt, float gravity,
                           float jumpSpeed) const {
    auto& pt = player.ensure<CTransform>();
    auto& pv = player.ensure<CVelocity>();
    auto& pp = player.ensure<CPhysics>();

    CAttack& atk = player.ensure<CAttack>();
    const bool diveLiftPhase = atk.isActive() && atk.isDiving && !atk.diveDropStarted;
    if (diveLiftPhase) {
        const float targetY = atk.diveLiftStartY + atk.diveLiftHeight;
        constexpr float kLiftSpeed = 14.f;
        constexpr float kDiveSpeed = -18.f;

        if (pt.pos.y < targetY - 1e-4f) {
            const float remaining = targetY - pt.pos.y;
            const float step = std::min(remaining, kLiftSpeed * dt);
            const float yBefore = pt.pos.y;
            pt.pos.y += step;
            pv.y = 0.f;
            resolveVerticalCollisions(player, platforms, obstacles, terrain);
            if (pt.pos.y <= yBefore + 1e-5f && step > 1e-5f) {
                atk.diveDropStarted = true;
                pv.y = kDiveSpeed;
            } else if (pt.pos.y >= targetY - 1e-4f) {
                pt.pos.y = targetY;
                atk.diveDropStarted = true;
                pv.y = kDiveSpeed;
            } else {
                pp.jumpReq = false;
                if (pt.pos.y < -10.f) {
                    pt.pos.y = -10.f;
                    pv.y = 0.f;
                    auto& hp = player.ensure<CHealth>();
                    if (hp.currentHp > 0) {
                        hp.currentHp = 0;
                        hp.invincTimer = 0.f;
                    }
                    return true;
                }
                return false;
            }
        } else {
            atk.diveDropStarted = true;
            pv.y = kDiveSpeed;
        }
        if (atk.diveDropStarted) pp.jumpReq = false;
    }

    const float g = (pv.y < 0.f) ? gravity * 1.5f : gravity;
    pv.y += g * dt;
    if (pv.y < -40.f) pv.y = -40.f;

    const bool wasOnGround = pp.onGround;
    const bool jumpingUp = pv.y > 0.1f;

    if (!wasOnGround) {
        pt.pos.x += pv.xz.x * dt;
        pt.pos.z += pv.xz.y * dt;
    }

    pt.pos.y += pv.y * dt;
    resolveVerticalCollisions(player, platforms, obstacles, terrain);

    if (wasOnGround && !pp.onGround && !jumpingUp && !pp.jumpReq) {
        if (trySnapToGround(pt, pv, wasOnGround, jumpingUp, pp.jumpReq, platforms, obstacles,
                              terrain)) {
            pp.onGround = true;
            pp.jumpsRemaining = pp.maxJumps;
            pp.usedDoubleJump = false;
        }
    }

    if (pp.jumpReq && pp.jumpsRemaining > 0) {
        if (pp.jumpsRemaining == 1) {
            pp.usedDoubleJump = true;
        }
        pv.y = jumpSpeed;
        pp.onGround = false;
        pp.jumpsRemaining--;
    }
    pp.jumpReq = false;

    if (pp.onGround) {
        const bool onMovingPlatform = pp.standingOn && pp.standingOn.is_alive() &&
                                        pp.standingOn.has<CMovingPlatform>();
        if (!onMovingPlatform) {
            pv.xz = glm::vec2{0.f};
        }
    }

    if (pt.pos.y < -10.f) {
        pt.pos.y = -10.f;
        pv.y = 0.f;
        auto& hp = player.ensure<CHealth>();
        if (hp.currentHp > 0) {
            hp.currentHp = 0;
            hp.invincTimer = 0.f;
        }
        return true;
    }
    return false;
}

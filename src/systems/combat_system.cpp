// =============================================================================
// combat_system.cpp — + slash SE (Slash / Smash / SmashDown)
// =============================================================================
// requestAttack / requestStrongAttack が実際に攻撃を開始したときだけ
// sound.playSlash() を発火 (= 既に isActive() で無視された場合は無音)。
// =============================================================================
#define NOMINMAX
#include "systems/combat_system.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "core/aabb.h"
#include "core/attack_registry.h"
#include "core/cylinder.h"
#include "core/game_state.h"
#include "core/grave.h"
#include "core/grave_fx.h"
#include "core/grip.h"
#include "core/obstacle.h"
#include "renderer/animation.h"
#include "renderer/asset_registry.h"
#include "systems/physics_util.h"
#include "systems/sound_manager.h"
#include "systems/spirit_system.h"
namespace {
constexpr float kDeg2Rad = 3.14159265358979323846f / 180.f;
constexpr float kDebugHitFlashDuration = 0.30f;
glm::mat3 makeYawRotation(float yawDeg) {
    const float r = yawDeg * kDeg2Rad;
    const float c = std::cos(r);
    const float s = std::sin(r);
    return glm::mat3{
        c, 0.f, -s,
        0.f, 1.f, 0.f,
        s, 0.f, c
    };
}
glm::vec3 cylinderMidCenter(const Cylinder& cyl) {
    return cyl.midCenter();
}
float attackerCenterY(const CTransform& t) {
    return t.pos.y + t.scale.y * 0.5f;
}
bool alreadyHit(const CAttack& atk, flecs::entity e) {
    return std::find(atk.hitEntities.begin(), atk.hitEntities.end(), e) != atk.hitEntities.end();
}
void startEnemyDying(WorldData& data, flecs::entity e) {
    if (!e.has<CEnemyAI>()) return;
    CEnemyAI& ai = e.ensure<CEnemyAI>();
    if (ai.isDying) return;
    ai.isDying = true;
    ai.punchActive = false;
    ai.moveVelocity = {0.f, 0.f};
    AssetRegistry& assets = data.vulkan.assets();
    const AnimationClip* clip = assets.getAnimation("enemy_death");
    if (!clip) clip = assets.getAnimation("death");
    constexpr float kEnemyDefaultDyingDuration = 1.5f;
    ai.dyingTimer = clip ? clip->duration : kEnemyDefaultDyingDuration;
    std::cout << "[Death] enemy '" << e.name().c_str() << "' dying duration="
              << ai.dyingTimer << "s\n";
}
void applyDamageToEnemy(WorldData& data, flecs::entity e, int damage, float invincDuration) {
    if (!e.has<CHealth>() || !e.has<CEnemyAI>()) return;
    CEnemyAI& ai = e.ensure<CEnemyAI>();
    if (ai.isDying) return;
    e.ensure<CHealth>().takeDamage(damage, false);
    ai.hitInvincTimer = invincDuration;
    ai.debugHitFlashTimer = kDebugHitFlashDuration;
    if (e.get<CHealth>().isDead()) {
        startEnemyDying(data, e);
    } else {
        constexpr float kEnemyHitReactDuration = 0.30f;
        ai.hitReactTimer = kEnemyHitReactDuration;
    }
}
float currentActiveProgress(const CAttack& atk) {
    const AttackDef& def = *atk.def;
    const float t = (atk.elapsed - def.activeStart()) / def.activeTime;
    return glm::clamp(t, 0.f, 1.f);
}
glm::vec3 computeCurrentWorldDir(const CAttack& atk, const CTransform& at) {
    const AttackDef& def = *atk.def;
    const float t = currentActiveProgress(atk);
    const glm::vec3 localDir = def.dirAt(t);
    const glm::mat3 yawRot = makeYawRotation(at.yaw);
    return yawRot * localDir;
}
float arcDistanceAngle(const glm::vec3& prevDir, const glm::vec3& currDir,
                        const glm::vec3& enemyDir) {
    const float prevDotCurr = glm::clamp(glm::dot(prevDir, currDir), -1.f, 1.f);
    if (prevDotCurr > 0.9999f) {
        const float d = glm::clamp(glm::dot(currDir, enemyDir), -1.f, 1.f);
        return std::acos(d);
    }
    const glm::vec3 cross = glm::cross(prevDir, currDir);
    const float crossLen = glm::length(cross);
    if (crossLen < 1e-5f) {
        const float d = glm::clamp(glm::dot(currDir, enemyDir), -1.f, 1.f);
        return std::acos(d);
    }
    const glm::vec3 N = cross / crossLen;
    const float ePlaneDot = glm::dot(enemyDir, N);
    const glm::vec3 ePlaneRaw = enemyDir - N * ePlaneDot;
    const float ePlaneLen = glm::length(ePlaneRaw);
    if (ePlaneLen < 1e-5f) {
        const float dPrev = glm::clamp(glm::dot(prevDir, enemyDir), -1.f, 1.f);
        const float dCurr = glm::clamp(glm::dot(currDir, enemyDir), -1.f, 1.f);
        return std::min(std::acos(dPrev), std::acos(dCurr));
    }
    const glm::vec3 enemyOnPlane = ePlaneRaw / ePlaneLen;
    const float prevDotEnemy = glm::clamp(glm::dot(prevDir, enemyOnPlane), -1.f, 1.f);
    const float currDotEnemy = glm::clamp(glm::dot(currDir, enemyOnPlane), -1.f, 1.f);
    const bool insideArc =
        (prevDotEnemy >= prevDotCurr) && (currDotEnemy >= prevDotCurr);
    if (insideArc) {
        const float planeOffsetClamped = glm::clamp(std::abs(ePlaneDot), 0.f, 1.f);
        return std::asin(planeOffsetClamped);
    }
    const float dPrev = glm::clamp(glm::dot(prevDir, enemyDir), -1.f, 1.f);
    const float dCurr = glm::clamp(glm::dot(currDir, enemyDir), -1.f, 1.f);
    return std::min(std::acos(dPrev), std::acos(dCurr));
}
void consumeGripOnAttackHit(flecs::entity attacker, CAttack& atk) {
    if (atk.gripConsumedThisAttack) return;
    atk.gripConsumedThisAttack = true;
    grip::consumeOnHit(attacker);
}
struct GraveGeom {
    glm::vec3 center;
    float radius;
};
GraveGeom getGraveGeom(flecs::entity grave) {
    const AABB box = physics::obstacleWorldAABB(grave);
    GraveGeom g{};
    g.center = box.center();
    g.radius = std::max(box.size().x, box.size().z) * 0.5f;
    return g;
}
void applyGraveDamage(WorldData& data, flecs::entity graveEntity, AttackKind kind) {
    if (!graveEntity.is_alive() || !graveEntity.has<CGrave>()) return;
    CGrave& g = graveEntity.ensure<CGrave>();
    if (g.state == CGrave::State::Destroyed) return;
    const CTransform& gt = graveEntity.get<CTransform>();
    const glm::vec3 spawnOrigin{gt.pos.x, gt.pos.y + gt.scale.y * 0.7f, gt.pos.z};
    int spawnCount = 0;
    bool destroyed = false;
    const char* fromStateName = nullptr;
    switch (g.state) {
        case CGrave::State::Intact:
            fromStateName = "Intact";
            if (kind == AttackKind::SmashDown) {
                spawnCount = 5;
                destroyed = true;
            } else {
                spawnCount = 2;
                g.state = CGrave::State::Damaged;
            }
            break;
        case CGrave::State::Damaged:
            fromStateName = "Damaged";
            if (kind == AttackKind::SmashDown) {
                spawnCount = 3;
                destroyed = true;
            } else {
                return;
            }
            break;
        case CGrave::State::Destroyed:
            return;
    }
    if (destroyed) {
        g.state = CGrave::State::Destroyed;
        g.destroyedElapsed = 0.f;
        if (graveEntity.has<CObstacle>()) {
            graveEntity.remove<CObstacle>();
        }
        if (graveEntity.has<ObstacleTag>()) {
            graveEntity.remove<ObstacleTag>();
        }
        data.obstacles.erase(
            std::remove(data.obstacles.begin(), data.obstacles.end(), graveEntity),
            data.obstacles.end());
    }
    for (int i = 0; i < spawnCount; ++i) {
        SpiritSystem::spawnSpirit(data, spawnOrigin);
    }
    grave_fx::syncEmitter(graveEntity);
    const char* attackName = (kind == AttackKind::Slash) ? "slash" :
                              (kind == AttackKind::Smash) ? "smash" : "smash_down";
    std::cout << "[Grave] '" << graveEntity.name().c_str() << "' " << fromStateName
              << " → " << (destroyed ? "Destroyed" : "Damaged") << " by " << attackName
              << ", spawned " << spawnCount << " spirits\n";
}
}  // namespace

void CombatSystem::requestAttack(flecs::entity attacker, SoundManager& sound) const {
    CAttack& atk = attacker.ensure<CAttack>();
    if (atk.isActive()) return;
    const bool isAerial = !attacker.get<CPhysics>().onGround;
    const AttackDef& def = attack_registry::get(AttackKind::Slash);
    startAttack(attacker, def, isAerial);
    // 振り始めで SE 再生
    sound.playSlash();
}

void CombatSystem::requestStrongAttack(flecs::entity attacker, SoundManager& sound) const {
    CAttack& atk = attacker.ensure<CAttack>();
    if (atk.isActive()) return;
    const CPhysics& pp = attacker.get<CPhysics>();
    const bool inAir = !pp.onGround;
    if (inAir && !pp.usedDoubleJump) return;
    const AttackKind kind = inAir ? AttackKind::SmashDown : AttackKind::Smash;
    const AttackDef& def = attack_registry::get(kind);
    startAttack(attacker, def, inAir);
    if (kind == AttackKind::SmashDown) {
        CAttack& a = attacker.ensure<CAttack>();
        a.isDiving = true;
        a.diveDropStarted = false;
        const CTransform& tr = attacker.get<CTransform>();
        a.diveLiftStartY = tr.pos.y;
        a.diveLiftHeight = tr.scale.y * 1.f;
        attacker.ensure<CVelocity>().y = 0.f;
    }
    // Smash / SmashDown どちらも slash.mp3 を流す
    sound.playSlash();
}
void CombatSystem::update(WorldData& data, flecs::entity attacker, float dt) const {
    CAttack& atk = attacker.ensure<CAttack>();
    if (!atk.isActive()) return;
    atk.elapsed += dt;
    const AttackDef& def = *atk.def;
    const float total = def.totalDuration();
    if (atk.elapsed >= def.activeStart() && atk.elapsed < def.activeEnd()) {
        const CTransform& t = attacker.get<CTransform>();
        const glm::vec3 currWorldDir = computeCurrentWorldDir(atk, t);
        if (glm::length(atk.prevSweepWorldDir) < 0.01f) {
            atk.prevSweepWorldDir = currWorldDir;
        } else {
            if (!atk.isDiving) {
                performSweepHit(data, attacker, atk, atk.prevSweepWorldDir, currWorldDir);
            }
            atk.prevSweepWorldDir = currWorldDir;
        }
    }
    if (atk.elapsed >= total) {
        endAttack(attacker);
    }
}
void CombatSystem::cancelAerialOnLanding(flecs::entity attacker, WorldData& data) const {
    CAttack& atk = attacker.ensure<CAttack>();
    if (!atk.isActive() || !atk.def) return;
    if (atk.def->kind == AttackKind::SmashDown && atk.isDiving &&
        !atk.landingShockwaveFired) {
        performShockwaveHit(data, attacker, atk);
        atk.landingShockwaveFired = true;
        return;
    }
    if (atk.isAerial && atk.def->canCancelOnLand) {
        endAttack(attacker);
    }
}
bool CombatSystem::isInputLocked(flecs::entity attacker) const {
    const CAttack& atk = attacker.get<CAttack>();
    if (!atk.isActive() || !atk.def) return false;
    if (atk.isDiving) return true;
    if (atk.isAerial) return false;
    return atk.def->lockMovementGround;
}
AttackPhase CombatSystem::getPhase(flecs::entity attacker) const {
    const CAttack& atk = attacker.get<CAttack>();
    if (!atk.isActive() || !atk.def) return AttackPhase::Idle;
    if (atk.elapsed < atk.def->windupEnd())     return AttackPhase::Windup;
    if (atk.elapsed < atk.def->activeEnd())     return AttackPhase::Active;
    if (atk.elapsed < atk.def->totalDuration()) return AttackPhase::Recovery;
    return AttackPhase::Idle;
}
glm::vec3 CombatSystem::getCurrentSweepWorldDir(flecs::entity attacker) const {
    const CAttack& atk = attacker.get<CAttack>();
    if (!atk.isActive() || !atk.def) return glm::vec3{0.f};
    if (atk.elapsed < atk.def->activeStart() || atk.elapsed >= atk.def->activeEnd())
        return glm::vec3{0.f};
    return computeCurrentWorldDir(atk, attacker.get<CTransform>());
}
void CombatSystem::startAttack(flecs::entity attacker, const AttackDef& def, bool isAerial) const {
    CAttack& atk = attacker.ensure<CAttack>();
    atk.def = &def;
    atk.elapsed = 0.f;
    atk.isAerial = isAerial;
    atk.isDiving = false;
    atk.diveLiftStartY = 0.f;
    atk.diveLiftHeight = 0.f;
    atk.diveDropStarted = false;
    atk.hitEntities.clear();
    atk.prevSweepWorldDir = glm::vec3{0.f};
    atk.landingShockwaveFired = false;
    atk.gripConsumedThisAttack = false;
}
void CombatSystem::endAttack(flecs::entity attacker) const {
    CAttack& atk = attacker.ensure<CAttack>();
    atk.def = nullptr;
    atk.elapsed = 0.f;
    atk.isAerial = false;
    atk.isDiving = false;
    atk.diveLiftStartY = 0.f;
    atk.diveLiftHeight = 0.f;
    atk.diveDropStarted = false;
    atk.hitEntities.clear();
    atk.prevSweepWorldDir = glm::vec3{0.f};
    atk.landingShockwaveFired = false;
    atk.gripConsumedThisAttack = false;
}
void CombatSystem::performSweepHit(WorldData& data, flecs::entity attacker, CAttack& atk,
                                     const glm::vec3& prevWorldDir,
                                     const glm::vec3& currWorldDir) const {
    const CTransform& at = attacker.get<CTransform>();
    const AttackDef& def = *atk.def;
    const glm::vec3 origin{at.pos.x, attackerCenterY(at), at.pos.z};
    const float halfWidthRad = def.halfWidthDeg * kDeg2Rad;
    data.world.each([&](flecs::entity e, const CTransform& et, const CEnemyAI& ai) {
        if (ai.isDying) return;
        if (ai.hitInvincTimer > 0.f) return;
        if (alreadyHit(atk, e)) return;
        const Cylinder ecyl = Cylinder::fromBottomCenter(et.pos, et.scale);
        const float enemyRadius = ecyl.radius;
        const glm::vec3 enemyCenter = cylinderMidCenter(ecyl);
        const glm::vec3 toEnemy = enemyCenter - origin;
        const float dist = glm::length(toEnemy);
        if (dist < 1e-4f) return;
        const float effectiveRange = def.range + enemyRadius;
        if (dist > effectiveRange) return;
        const glm::vec3 enemyDir = toEnemy / dist;
        const float arcAngle = arcDistanceAngle(prevWorldDir, currWorldDir, enemyDir);
        const float radiusRatio = glm::clamp(enemyRadius / dist, 0.f, 0.999f);
        const float angularRadius = std::asin(radiusRatio);
        if (arcAngle - angularRadius <= halfWidthRad) {
            atk.hitEntities.push_back(e);
            applyDamageToEnemy(data, e, def.damage, def.totalDuration());
            consumeGripOnAttackHit(attacker, atk);
        }
    });
    std::vector<flecs::entity> graveHits;
    data.world.each([&](flecs::entity e, const CTransform& /*et*/, const CGrave& g) {
        if (g.state == CGrave::State::Destroyed) return;
        if (alreadyHit(atk, e)) return;
        const GraveGeom gg = getGraveGeom(e);
        const glm::vec3 toGrave = gg.center - origin;
        const float dist = glm::length(toGrave);
        if (dist < 1e-4f) return;
        const float effectiveRange = def.range + gg.radius;
        if (dist > effectiveRange) return;
        const glm::vec3 graveDir = toGrave / dist;
        const float arcAngle = arcDistanceAngle(prevWorldDir, currWorldDir, graveDir);
        const float radiusRatio = glm::clamp(gg.radius / dist, 0.f, 0.999f);
        const float angularRadius = std::asin(radiusRatio);
        if (arcAngle - angularRadius <= halfWidthRad) {
            graveHits.push_back(e);
        }
    });
    for (flecs::entity e : graveHits) {
        atk.hitEntities.push_back(e);
        applyGraveDamage(data, e, def.kind);
        consumeGripOnAttackHit(attacker, atk);
    }
}
void CombatSystem::performShockwaveHit(WorldData& data, flecs::entity attacker,
                                         CAttack& atk) const {
    const CTransform& at = attacker.get<CTransform>();
    const AttackDef& def = *atk.def;
    constexpr float kDownDuration = 3.f;
    const float atkCenterY = attackerCenterY(at);
    const float rangeSq = def.range * def.range;
    const float atkYMin = atkCenterY - 2.0f;
    const float atkYMax = atkCenterY + 2.0f;
    data.world.each([&](flecs::entity e, const CTransform& et, CEnemyAI& ai) {
        if (ai.isDying) return;
        if (ai.hitInvincTimer > 0.f) return;
        if (alreadyHit(atk, e)) return;
        const Cylinder ecyl = Cylinder::fromBottomCenter(et.pos, et.scale);
        if (!cylinder::overlapsYRange(ecyl, atkYMin, atkYMax)) return;
        const float dx = et.pos.x - at.pos.x;
        const float dz = et.pos.z - at.pos.z;
        const float distSq = dx * dx + dz * dz;
        if (distSq > rangeSq) return;
        atk.hitEntities.push_back(e);
        ai.isDown = true;
        ai.downTimer = kDownDuration;
        ai.state = EnemyState::Cooldown;
        ai.punchActive = false;
        applyDamageToEnemy(data, e, def.damage, def.totalDuration());
        consumeGripOnAttackHit(attacker, atk);
    });
    std::vector<flecs::entity> graveHits;
    data.world.each([&](flecs::entity e, const CTransform& /*et*/, const CGrave& g) {
        if (g.state == CGrave::State::Destroyed) return;
        if (alreadyHit(atk, e)) return;
        const GraveGeom gg = getGraveGeom(e);
        if (gg.center.y < atkYMin || gg.center.y > atkYMax) return;
        const float dx = gg.center.x - at.pos.x;
        const float dz = gg.center.z - at.pos.z;
        const float distSq = dx * dx + dz * dz;
        const float effRange = def.range + gg.radius;
        if (distSq > effRange * effRange) return;
        graveHits.push_back(e);
    });
    for (flecs::entity e : graveHits) {
        atk.hitEntities.push_back(e);
        applyGraveDamage(data, e, def.kind);
        consumeGripOnAttackHit(attacker, atk);
    }
}

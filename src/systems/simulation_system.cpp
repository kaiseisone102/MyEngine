// =============================================================================
// simulation_system.cpp — + ItemPickupSystem::update に sound 渡す
// =============================================================================
#include "systems/simulation_system.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "core/aabb.h"
#include "core/action_state.h"
#include "core/components.h"
#include "core/cylinder.h"
#include "core/enemy_hitbox_util.h"
#include "core/equipment_util.h"
#include "core/grave.h"
#include "core/spawn_trigger.h"
#include "renderer/animation.h"
#include "renderer/asset_registry.h"
#include "systems/physics_util.h"
namespace {
constexpr float kEnemyFallKillY = -30.f;
}  // namespace
bool SimulationSystem::isGroundEnemy(flecs::entity e) {
    return e.has<SkeletonTag>() || e.has<SoldierTag>();
}
void SimulationSystem::applyDamageToPlayer(GameState& s, int amount) {
    flecs::entity player = s.worldState.data.player;
    CHealth& hp = player.ensure<CHealth>();
    if (hp.isInvincible()) return;
    if (player.has<CShield>()) {
        CShield& sh = player.ensure<CShield>();
        if (sh.guarding && sh.canGuard()) {
            sh.durability--;
            hp.invincTimer = CHealth::kInvincTime;
            std::cout << "[Shield] guarded! durability=" << sh.durability << "/"
                      << CShield::maxDurability(sh.type) << "\n";
            if (sh.durability <= 0) {
                AssetRegistry& assets = s.worldState.data.vulkan.assets();
                equipment::applyShieldChange(player, assets, ShieldType::None);
                std::cout << "[Shield] broken! shield lost\n";
                sh.guarding = false;
            }
            return;
        }
    }
    hp.takeDamage(amount);
}
void SimulationSystem::doRespawn(GameState& s) {
    s.worldState.data.player.ensure<CTransform>().pos = {0.f, 0.f, 0.f};
    s.worldState.data.player.ensure<CVelocity>().y = 0.f;
    s.worldState.data.player.ensure<CVelocity>().xz = glm::vec2{0.f};
    s.worldState.data.player.ensure<CHealth>().respawn();
    AssetRegistry& assets = s.worldState.data.vulkan.assets();
    equipment::applyShieldChange(s.worldState.data.player, assets, ShieldType::Iron);
    std::cout << "[Player Dead] Respawn (shield restored: Iron)\n";
}
void SimulationSystem::updateEnemy(GameState& s, float dt, float gravity) {
    auto& wd = s.worldState.data;
    auto& ws = s.worldState.systems;
    ws.spawnSystem.update(wd.spawnTriggers, wd.player, wd.enemies, wd.world, wd.vulkan);
    ws.enemySystem.update(wd.enemies, wd.player, dt);
    for (flecs::entity e : wd.enemies) {
        if (!isGroundEnemy(e)) continue;
        ws.physicsSystem.applyEnemyGravity(e, wd.platforms, &wd.terrains, dt, gravity);
    }
    for (flecs::entity e : wd.enemies) {
        if (!isGroundEnemy(e)) continue;
        ws.movementSystem.moveWithSlide(e, e.get<CEnemyAI>().moveVelocity, wd.platforms);
    }
    for (flecs::entity e : wd.enemies) {
        if (!isGroundEnemy(e)) continue;
        ws.movementSystem.resolveEntityVsEntity(wd.player, e);
    }
    for (size_t i = 0; i < wd.enemies.size(); ++i) {
        if (!isGroundEnemy(wd.enemies[i])) continue;
        for (size_t j = i + 1; j < wd.enemies.size(); ++j) {
            if (!isGroundEnemy(wd.enemies[j])) continue;
            ws.movementSystem.resolveEntityVsEntity(wd.enemies[i], wd.enemies[j]);
        }
    }
    {
        const Cylinder pcyl = physics::entityCylinder(wd.player);
        wd.enemies.erase(std::remove_if(wd.enemies.begin(), wd.enemies.end(),
                                        [&](flecs::entity e) {
                                            if (!e.has<GhostTag>()) return false;
                                            const Cylinder gcyl = physics::entityCylinder(e);
                                            if (!cylinder::overlap(pcyl, gcyl)) return false;
                                            applyDamageToPlayer(s, 1);
                                            e.destruct();
                                            return true;
                                        }),
                         wd.enemies.end());
    }
    {
        const Cylinder pcyl = physics::entityCylinder(wd.player);
        for (flecs::entity e : wd.enemies) {
            if (!isGroundEnemy(e)) continue;
            const CEnemyAI& ai = e.get<CEnemyAI>();
            if (!ai.punchActive) continue;
            const CTransform& et = e.get<CTransform>();
            const bool skel = e.has<SkeletonTag>();
            const auto ph = enemy_hitbox::makeGroundPunch(et, ai, skel);
            const AABB hitbox = AABB::fromCenterHalf(ph.center, ph.half);
            if (!cylinder::overlap(pcyl, hitbox)) continue;
            applyDamageToPlayer(s, 1);
            break;
        }
    }
    {
        wd.enemies.erase(std::remove_if(wd.enemies.begin(), wd.enemies.end(),
                                        [&](flecs::entity e) {
                                            if (!e.has<CTransform>()) return false;
                                            const CTransform& t = e.get<CTransform>();
                                            if (t.pos.y >= kEnemyFallKillY) return false;
                                            std::cout << "[FallKill] enemy '" << e.name().c_str()
                                                      << "' y=" << t.pos.y << " destructed\n";
                                            e.destruct();
                                            return true;
                                        }),
                         wd.enemies.end());
    }
}
void SimulationSystem::updatePlayer(GameState& s, const ActionState& input, float dt, float gravity,
                                    float jumpSpeed) {
    auto& wd = s.worldState.data;
    auto& ws = s.worldState.systems;
    ws.animStateSystem.update(s, dt, input);
    ws.skeletalAnimSystem.update(s, dt);
    ws.itemPhysicsSystem.update(wd, dt, gravity);
    // ─── アイテム拾い (SoundManager 注入) ────
    ws.itemPickupSystem.update(wd.shieldItems, wd.armorItems, wd.gripItems, wd.keyItems,
                               wd.moneyItems, wd.potionItems, wd.spiritItems, wd.player,
                               wd.vulkan.assets(), ws.sound);
    wd.player.ensure<CHealth>().tick(dt);
    ws.combatSystem.update(wd, wd.player, dt);
    wd.enemies.erase(std::remove_if(wd.enemies.begin(), wd.enemies.end(),
                                    [&](flecs::entity e) {
                                        if (!e.has<CEnemyAI>()) return false;
                                        const CEnemyAI& ai = e.get<CEnemyAI>();
                                        if (!ai.isDying) return false;
                                        if (ai.dyingTimer > 0.f) return false;
                                        std::cout << "[Death] enemy '" << e.name().c_str()
                                                  << "' anim complete, destructing\n";
                                        e.destruct();
                                        return true;
                                    }),
                     wd.enemies.end());
    const bool isDeadAnim =
        wd.player.has<CAnimState>() && wd.player.get<CAnimState>().current == AnimState::Dead;
    const bool inputLocked = ws.combatSystem.isInputLocked(wd.player) || isDeadAnim;
    if (wd.player.has<CShield>()) {
        wd.player.ensure<CShield>().guarding = input.guardHeld;
    }
    ws.movingPlatformSystem.update(wd, dt);
    ws.gateSystem.update(wd, dt, ws.sound);
    ws.chestSystem.update(wd, dt);
    {
        std::vector<flecs::entity> graveToDestruct;
        wd.world.each([&](flecs::entity e, CGrave& g) {
            if (g.state != CGrave::State::Destroyed) return;
            g.destroyedElapsed += dt;
            if (g.destroyedElapsed >= CGrave::kDestructTime) {
                graveToDestruct.push_back(e);
            }
        });
        for (flecs::entity e : graveToDestruct) {
            if (!e.is_alive()) continue;
            std::cout << "[Grave] destructing faded grave '" << e.name().c_str() << "'\n";
            wd.graves.erase(std::remove(wd.graves.begin(), wd.graves.end(), e), wd.graves.end());
            e.destruct();
        }
    }
    ws.spiritSystem.update(wd, dt);
    if (!inputLocked) {
        ws.movementSystem.updateTpsPlayerMove(wd.player, s.runtime.camera, input, &wd.terrains, dt);
    }
    ws.movementSystem.resolveHorizontalCollisions(wd.player, wd.platforms);
    ws.movementSystem.resolveObstacleCollisions(wd.player, wd.obstacles);
    ws.physicsSystem.update(wd.player, wd.platforms, wd.obstacles, &wd.terrains, dt, gravity,
                            jumpSpeed);
    if (wd.player.get<CPhysics>().onGround) {
        ws.combatSystem.cancelAerialOnLanding(wd.player, wd);
    }
    ws.audioEventSystem.onPostPhysics(wd.player, ws.sound);
    ws.cameraSystem.updateFpsMove(s.runtime.camera, inputLocked, input, dt);
}

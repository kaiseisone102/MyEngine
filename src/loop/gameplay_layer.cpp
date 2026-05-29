// =============================================================================
// gameplay_layer.cpp  E+ chest 対忁E//
// =============================================================================
#define NOMINMAX
#include "loop/gameplay_layer.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/aabb.h"
#include "core/action_state.h"
#include "core/attack_def.h"
#include "core/chest.h"
#include "core/components.h"
#include "core/cylinder.h"
#include "core/enemy_hitbox_util.h"
#include "core/gate.h"
#include "core/grip.h"
#include "core/key.h"
#include "core/money.h"
#include "core/spawn_trigger.h"
#include "loop/layer_factory.h"
#include "renderer/animation.h"
#include "renderer/animator.h"
#include "renderer/debug_line_renderer.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/vulkan_renderer.h"
#include "scene/scene_renderer.h"
#include "systems/hud_system.h"
#include "systems/physics_util.h"
#include "systems/render_debug_system.h"
#include "systems/spawn_system.h"
#include "world/stage_def.h"

namespace {

constexpr float kWarpInvincDuration = 1.0f;
constexpr float kPlayerInvincBlinkHz = 5.f;

constexpr int kActiveEdgeLayerCount = 5;
constexpr float kActiveEdgeStepMeters = 0.01f;

DebugOverlayData buildDebugOverlayData(GameState& gameState) {
    int entityCount = 0;
    gameState.worldState.data.world.each([&](const CTransform&) { ++entityCount; });

    const CHealth& hp = gameState.worldState.data.player.get<CHealth>();
    const CAttack& atk = gameState.worldState.data.player.get<CAttack>();
    const CShield& sh = gameState.worldState.data.player.has<CShield>()
                            ? gameState.worldState.data.player.get<CShield>()
                            : CShield{};

    const SkinBufferPool& pool = gameState.worldState.data.vulkan.skinBufferPool();

    DebugOverlayData d{};
    d.entityCount = entityCount;
    d.fps = gameState.runtime.fps;
    d.playerPos = gameState.worldState.data.player.get<CTransform>().pos;
    d.velY = gameState.worldState.data.player.get<CVelocity>().y;
    d.onGround = gameState.worldState.data.player.get<CPhysics>().onGround;
    d.isTps = gameState.runtime.camera.mode == CameraMode::TPS;
    d.attackActive = atk.isActive();
    d.attackTime = atk.elapsed;
    d.hpSegments = hp.segmentCount;
    d.hpUnlocked = hp.unlockedSegments;
    d.hpCurrentHp = hp.currentHp;
    d.invincTimer = hp.invincTimer;
    d.shieldType = sh.type;
    d.shieldDurability = sh.durability;
    d.skinnedAllocated = static_cast<int>(pool.allocatedCount());
    d.skinnedCapacity = static_cast<int>(pool.capacity());
    d.instancedVisible = gameState.worldState.data.vulkan.instancedVisible();
    d.instancedTotal = gameState.worldState.data.vulkan.instancedTotal();

    if (gameState.worldState.data.player.has<CGrip>()) {
        const CGrip& g = gameState.worldState.data.player.get<CGrip>();
        d.gripType = g.type;
        d.gripDurability = g.durability;
        d.gripMaxDurability = grip::def(g.type).maxDurability;
    } else {
        d.gripType = GripType::None;
        d.gripDurability = 0;
        d.gripMaxDurability = 0;
    }

    if (gameState.worldState.data.player.has<CKeyInventory>()) {
        const CKeyInventory& inv = gameState.worldState.data.player.get<CKeyInventory>();
        d.goldKeys = inv.goldKeys;
        d.silverKeys = inv.silverKeys;
    } else {
        d.goldKeys = 0;
        d.silverKeys = 0;
    }

    if (gameState.worldState.data.player.has<CMoney>()) {
        d.money = gameState.worldState.data.player.get<CMoney>().amount;
    } else {
        d.money = 0;
    }

    const KeyMapping& m = gameState.settings.keyMapping;
    d.moveKeysLabel = m.moveForward.shortName() + "/" + m.moveLeft.shortName() + "/" +
                      m.moveBack.shortName() + "/" + m.moveRight.shortName();
    d.jumpKeyLabel = m.jump.shortName();
    d.attackKeyLabel = m.attack.shortName();
    d.strongAttackKeyLabel = m.strongAttack.shortName();
    d.toggleCameraKeyLabel = m.toggleCamera.shortName();

    d.topLayerName = gameState.runtime.topLayerName;
    d.layerStackDepth = gameState.runtime.layerStackDepth;

    return d;
}

void debugSpawnBurst(GameState& gameState, int count) {
    auto& wd = gameState.worldState.data;
    const glm::vec3& playerPos = wd.player.get<CTransform>().pos;

    int spawned = 0;
    int failed = 0;
    static int s_burstIndex = 0;

    for (int i = 0; i < count; ++i) {
        const float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
        const float dist = 4.f + static_cast<float>(rand()) / RAND_MAX * 4.f;
        const glm::vec3 spawnPos{playerPos.x + std::cos(angle) * dist, 0.f,
                                 playerPos.z + std::sin(angle) * dist};

        const std::string name = "burst_" + std::to_string(s_burstIndex++);
        const bool useSoldier = (i % 2) == 1;

        if (useSoldier) {
            flecs::entity e = wd.world.entity(name.c_str())
                                  .set<CTransform>({spawnPos, 0.f, {0.6f, 1.0f, 0.6f}})
                                  .set<CEnemyAI>({EnemyState::Chase, 2.0f})
                                  .set<CVelocity>({0.f})
                                  .set<CHealth>({1, 2})
                                  .add<EnemyTag>()
                                  .add<SoldierTag>();
            auto& ai = e.ensure<CEnemyAI>();
            ai.spawnStunTimer = 0.5f;
            ai.attackStartupLag = 0.10f;
            SpawnSystem::attachSkeletalAnim(e, wd.vulkan, "soldier");

            if (e.has<CSkeletalAnim>() && e.get<CSkeletalAnim>().skinSlot.valid()) {
                wd.enemies.push_back(e);
                spawned++;
            } else {
                e.destruct();
                failed++;
            }
        } else {
            flecs::entity e = SpawnSystem::createSkeleton(wd.world, name.c_str(), spawnPos,
                                                          &wd.enemies, 0.5f, &wd.vulkan);
            if (e.has<CSkeletalAnim>() && e.get<CSkeletalAnim>().skinSlot.valid()) {
                spawned++;
            } else {
                wd.enemies.erase(std::remove(wd.enemies.begin(), wd.enemies.end(), e),
                                 wd.enemies.end());
                e.destruct();
                failed++;
            }
        }
    }

    std::cout << "[Debug F12] Burst spawn: " << spawned << " succeeded, " << failed
              << " failed (Pool: " << wd.vulkan.skinBufferPool().allocatedCount() << "/"
              << wd.vulkan.skinBufferPool().capacity() << ")\n";
}

void debugClearEnemies(GameState& gameState) {
    auto& wd = gameState.worldState.data;
    const size_t before = wd.enemies.size();
    for (flecs::entity e : wd.enemies) {
        if (e.is_alive()) e.destruct();
    }
    wd.enemies.clear();
    std::cout << "[Debug F11] Cleared " << before
              << " enemies (Pool: " << wd.vulkan.skinBufferPool().allocatedCount() << "/"
              << wd.vulkan.skinBufferPool().capacity() << ")\n";
}

void debugLogPoolStatus(GameState& gameState) {
    const SkinBufferPool& pool = gameState.worldState.data.vulkan.skinBufferPool();
    int entityCount = 0;
    gameState.worldState.data.world.each([&](const CTransform&) { ++entityCount; });

    std::cout << "[Debug F10] SkinBufferPool: " << pool.allocatedCount() << " / "
              << pool.capacity() << " | Entities: " << entityCount
              << " | Enemies: " << gameState.worldState.data.enemies.size() << " | FPS: " << gameState.runtime.fps
              << "\n";
}

void debugDumpAnimators(GameState& gameState) {
    std::cout << "\n[Debug F9] === Animator State Dump ===\n";
    int count = 0;
    gameState.worldState.data.world.each([&](flecs::entity e, CSkeletalAnim& sa) {
        const char* name = e.name().c_str();
        if (!name || name[0] == '\0') name = "(unnamed)";

        const AnimationClip* clip = sa.animator.currentClip();
        const char* clipName = (clip && !clip->name.empty()) ? clip->name.c_str() : "(null)";

        std::printf("  %-20s  clip='%-14s'  slot=%u\n", name, clipName, sa.skinSlot.boneOffset);
        count++;
    });
    std::cout << "[Debug F9] Total: " << count << "\n\n";
}

RenderDebugSystem::Corner nextCorner(RenderDebugSystem::Corner c) {
    using C = RenderDebugSystem::Corner;
    switch (c) {
        case C::BottomLeft:
            return C::BottomRight;
        case C::BottomRight:
            return C::TopRight;
        case C::TopRight:
            return C::BottomLeft;
        case C::TopLeft:
            return C::BottomLeft;
    }
    return C::BottomLeft;
}

const char* cornerName(RenderDebugSystem::Corner c) {
    using C = RenderDebugSystem::Corner;
    switch (c) {
        case C::BottomLeft:
            return "BottomLeft";
        case C::BottomRight:
            return "BottomRight";
        case C::TopRight:
            return "TopRight";
        case C::TopLeft:
            return "TopLeft";
    }
    return "?";
}

void cancelGuard(GameState& gameState) {
    auto& player = gameState.worldState.data.player;
    if (player && player.is_alive() && player.has<CShield>()) {
        player.ensure<CShield>().guarding = false;
    }
}

bool playerCanOpenGate(flecs::entity player, flecs::entity gate) {
    if (!gate || !gate.is_alive() || !gate.has<CGate>()) return false;
    const CGate& g = gate.get<CGate>();
    if (!g.requiresKey) return true;
    if (!player || !player.is_alive() || !player.has<CKeyInventory>()) return false;
    return player.get<CKeyInventory>().has(g.requiredKey);
}

bool playerCanOpenChest(flecs::entity player, flecs::entity chest) {
    if (!chest || !chest.is_alive() || !chest.has<CChest>()) return false;
    const CChest& c = chest.get<CChest>();
    if (!c.requiresKey) return true;
    if (!player || !player.is_alive() || !player.has<CKeyInventory>()) return false;
    return player.get<CKeyInventory>().has(c.requiredKey);
}

void drawCylinderWire(DebugLineRenderer& dl, const Cylinder& c, const glm::vec4& edgeColor,
                      const glm::vec4& sideColor) {
    const float r = c.radius;
    const float bot = c.bottomY();
    const float top = c.topY();
    const float mid = c.midY();
    const float cx = c.baseCenter.x;
    const float cz = c.baseCenter.z;

    dl.addCircleXZ({cx, bot, cz}, r, edgeColor, 32);
    dl.addCircleXZ({cx, top, cz}, r, edgeColor, 32);
    dl.addCircleXZ({cx, mid, cz}, r, sideColor, 24);

    dl.addLine({cx + r, bot, cz}, {cx + r, top, cz}, sideColor);
    dl.addLine({cx - r, bot, cz}, {cx - r, top, cz}, sideColor);
    dl.addLine({cx, bot, cz + r}, {cx, top, cz + r}, sideColor);
    dl.addLine({cx, bot, cz - r}, {cx, top, cz - r}, sideColor);
}

void drawEnemyHitboxDebug(DebugLineRenderer& dl, const CTransform& et, const CEnemyAI& ai) {
    const Cylinder cyl = Cylinder::fromBottomCenter(et.pos, et.scale);

    const bool hitFlash = (ai.debugHitFlashTimer > 0.f);

    const glm::vec4 edgeColor =
        hitFlash ? glm::vec4{1.f, 0.2f, 0.2f, 1.0f} : glm::vec4{0.f, 0.7f, 1.0f, 1.0f};
    const glm::vec4 sideColor =
        hitFlash ? glm::vec4{1.f, 0.4f, 0.4f, 0.8f} : glm::vec4{0.f, 0.5f, 0.8f, 0.8f};

    drawCylinderWire(dl, cyl, edgeColor, sideColor);
}

void drawAABBEdges(DebugLineRenderer& dl, const glm::vec3& center, const glm::vec3& half,
                   const glm::vec4& color) {
    const glm::vec3 mn = center - half;
    const glm::vec3 mx = center + half;

    const glm::vec3 v[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
    };

    dl.addLine(v[0], v[1], color);
    dl.addLine(v[1], v[5], color);
    dl.addLine(v[5], v[4], color);
    dl.addLine(v[4], v[0], color);
    dl.addLine(v[2], v[3], color);
    dl.addLine(v[3], v[7], color);
    dl.addLine(v[7], v[6], color);
    dl.addLine(v[6], v[2], color);
    dl.addLine(v[0], v[2], color);
    dl.addLine(v[1], v[3], color);
    dl.addLine(v[4], v[6], color);
    dl.addLine(v[5], v[7], color);
}

void drawAABBEdgesThick(DebugLineRenderer& dl, const glm::vec3& center, const glm::vec3& half,
                        const glm::vec4& color) {
    for (int i = 0; i < kActiveEdgeLayerCount; ++i) {
        const float offset = kActiveEdgeStepMeters * static_cast<float>(i);
        const glm::vec3 expandedHalf = half + glm::vec3{offset};
        drawAABBEdges(dl, center, expandedHalf, color);
    }
}

void drawAABBWithCenter(DebugLineRenderer& dl, const AABB& box, const glm::vec4& edgeColor,
                        const glm::vec4& centerColor, bool thickEdges = false) {
    const glm::vec3 c = box.center();
    const glm::vec3 h = box.half();

    if (thickEdges) {
        drawAABBEdgesThick(dl, c, h, edgeColor);
    } else {
        drawAABBEdges(dl, c, h, edgeColor);
    }

    const float armLen = std::min({h.x, h.y, h.z}) * 0.6f;
    dl.addLine({c.x - armLen, c.y, c.z}, {c.x + armLen, c.y, c.z}, centerColor);
    dl.addLine({c.x, c.y - armLen, c.z}, {c.x, c.y + armLen, c.z}, centerColor);
    dl.addLine({c.x, c.y, c.z - armLen}, {c.x, c.y, c.z + armLen}, centerColor);
}

void drawEnemyAttackHitboxDebug(DebugLineRenderer& dl, const CTransform& et, const CEnemyAI& ai,
                                bool isSkeleton) {
    const auto ph = enemy_hitbox::makeGroundPunch(et, ai, isSkeleton);
    const AABB box = AABB::fromCenterHalf(ph.center, ph.half);

    const bool active = ai.punchActive;

    const glm::vec4 edgeColor =
        active ? glm::vec4{0.10f, 0.20f, 1.00f, 1.00f} : glm::vec4{0.40f, 0.50f, 1.00f, 0.35f};
    const glm::vec4 centerColor =
        active ? glm::vec4{0.40f, 0.60f, 1.00f, 1.00f} : glm::vec4{0.50f, 0.70f, 1.00f, 0.50f};

    drawAABBWithCenter(dl, box, edgeColor, centerColor, /*thickEdges=*/active);
}

void drawPlayerHurtboxDebug(DebugLineRenderer& dl, flecs::entity player, float elapsedTime) {
    if (!player || !player.is_alive()) return;
    if (!player.has<CHealth>()) return;

    const Cylinder cyl = physics::entityCylinder(player);
    const CHealth& hp = player.get<CHealth>();
    const bool invinc = (hp.invincTimer > 0.f);

    glm::vec4 edgeColor;
    glm::vec4 sideColor;

    if (invinc) {
        const float phase = elapsedTime * kPlayerInvincBlinkHz * 6.2831853f;
        const float blink = 0.3f + 0.7f * std::abs(std::sin(phase));
        edgeColor = glm::vec4{1.0f, 1.0f, 0.2f, blink};
        sideColor = glm::vec4{1.0f, 1.0f, 0.5f, blink};
    } else {
        edgeColor = glm::vec4{0.20f, 1.00f, 0.30f, 1.00f};
        sideColor = glm::vec4{0.50f, 1.00f, 0.60f, 0.80f};
    }

    drawCylinderWire(dl, cyl, edgeColor, sideColor);
}

glm::vec3 tiltStartDir(const glm::vec3& startDir, const glm::vec3& rotationAxis, float tiltDeg) {
    const glm::vec3 tiltAxisRaw = glm::cross(rotationAxis, startDir);
    const float len = glm::length(tiltAxisRaw);
    if (len < 1e-4f) return startDir;
    const glm::vec3 tiltAxis = tiltAxisRaw / len;
    const glm::quat q = glm::angleAxis(glm::radians(tiltDeg), tiltAxis);
    return q * startDir;
}

}  // namespace

GameplayLayer::GameplayLayer(const LayerContext& ctx, float gravity, float jumpSpeed,
                             StageId initialStage)
    : state_(ctx.state),
      renderer_(ctx.sceneRenderer),
      vulkan_(ctx.vulkan),
      factory_(ctx.factory),
      gravity_(gravity),
      jumpSpeed_(jumpSpeed),
      currentStage_(initialStage) {}

GameplayLayer::~GameplayLayer() = default;

void GameplayLayer::onEnter() {
    const StageDef& def = stage_registry::get(currentStage_);
    std::cout << "[GameplayLayer] enter ↁEbuild stage: " << def.name << "\n";
    worldBuilder_.build(state_.worldState.data, currentStage_, /*keepPlayer=*/false);
    state_.worldState.systems.audioEventSystem.syncGroundState(state_.worldState.data.player);
    deathTimer_ = 0.f;
    gameOverPushed_ = false;
    nearbyWarp_ = flecs::entity::null();
    nearbyGate_ = flecs::entity::null();
    nearbyChest_ = flecs::entity::null();
    debugElapsedTime_ = 0.f;
}

void GameplayLayer::onExit() {
    std::cout << "[GameplayLayer] exit ↁEworld reset\n";
    worldBuilder_.reset(state_.worldState.data, /*keepPlayer=*/false);
}

void GameplayLayer::requestWarpToStage(StageId target) {
    pendingWarp_ = true;
    pendingWarpTarget_ = target;
    std::cout << "[GameplayLayer] warp requested -> " << stage_registry::get(target).name
              << " (deferred to next frame)\n";
}

void GameplayLayer::warpToStage(StageId target) {
    const StageDef& def = stage_registry::get(target);
    std::cout << "[GameplayLayer] warp executing -> " << def.name
              << " (player state will be kept)\n";

    worldBuilder_.reset(state_.worldState.data, /*keepPlayer=*/true);
    worldBuilder_.build(state_.worldState.data, target, /*keepPlayer=*/true);

    if (state_.worldState.data.player && state_.worldState.data.player.is_alive() &&
        state_.worldState.data.player.has<CHealth>()) {
        auto& hp = state_.worldState.data.player.ensure<CHealth>();
        if (hp.invincTimer < kWarpInvincDuration) {
            hp.invincTimer = kWarpInvincDuration;
        }
    }

    state_.worldState.systems.audioEventSystem.syncGroundState(state_.worldState.data.player);
    currentStage_ = target;
    deathTimer_ = 0.f;
    gameOverPushed_ = false;
    nearbyWarp_ = flecs::entity::null();
    nearbyGate_ = flecs::entity::null();
    nearbyChest_ = flecs::entity::null();
    std::cout << "[GameplayLayer] warp done -> " << def.name << "\n";
}

void GameplayLayer::pushPauseMenu(LayerCommands& cmds) {
    cmds.requestPush(factory_.createChoiceOverlay(
        "Pause", {"Resume", "Warp", "Title"},
        [this](int idx, LayerCommands& c) {
            if (idx == 0) {
                c.requestPop();
            } else if (idx == 1) {
                std::vector<std::string> choices;
                std::vector<StageId> targetIds;
                for (const auto& def : stage_registry::all()) {
                    choices.push_back(def.name);
                    targetIds.push_back(def.id);
                }
                choices.push_back("Cancel");

                c.requestPush(factory_.createChoiceOverlay(
                    "Warp to?", std::move(choices),
                    [this, targetIds](int idx2, LayerCommands& c2) {
                        c2.requestPop();
                        if (idx2 >= 0 && idx2 < static_cast<int>(targetIds.size())) {
                            c2.requestPop();
                            requestWarpToStage(targetIds[idx2]);
                        }
                    },
                    MenuLayerBase::MenuLayout::Vertical));
            } else if (idx == 2) {
                c.requestReplace(factory_.createTitleLayer());
            }
        },
        MenuLayerBase::MenuLayout::Vertical));
}

void GameplayLayer::handleEvents(const EventBus& events, LayerCommands& cmds) {
    auto& gameState = state_;
    auto& ws = gameState.worldState;
    DebugFlags& dbg = gameState.runtime.debug;

    if (findEvent<MenuBackRequested>(events)) {
        pushPauseMenu(cmds);
        return;
    }

    for (const GameEvent& ev : events.events()) {
        if (std::holds_alternative<QuitRequested>(ev)) {
            cmds.requestQuit();
            continue;
        }
        if (std::holds_alternative<WindowResizeRequested>(ev)) {
            ws.data.vulkan.onResize();
            continue;
        }
        if (std::holds_alternative<ToggleCameraRequested>(ev)) {
            ws.systems.cameraSystem.toggleMode(gameState.runtime.camera);
            continue;
        }
        if (std::holds_alternative<JumpRequested>(ev)) {
            cancelGuard(gameState);
            if (!ws.systems.combatSystem.isInputLocked(ws.data.player)) {
                ws.data.player.ensure<CPhysics>().jumpReq = true;
            }
            ws.systems.audioEventSystem.onJumpRequested(
                ws.data.player, ws.systems.combatSystem.isInputLocked(ws.data.player),
                ws.systems.sound);
            continue;
        }
        if (std::holds_alternative<AttackRequested>(ev)) {
            // 優先度: Chest > Gate > WarpPad > 通常攻撁E            // Chest/Gate
            // 鍵不足の場合は通常攻撃へフォールスルー
            if (nearbyChest_ && nearbyChest_.is_alive()) {
                const auto result =
                    ws.systems.chestSystem.tryOpenNearestChest(ws.data, ws.systems.sound);
                if (result == ChestSystem::OpenResult::Opened) {
                    continue;
                }
            }
            if (nearbyGate_ && nearbyGate_.is_alive()) {
                const auto result =
                    ws.systems.gateSystem.tryOpenNearestGate(ws.data, ws.systems.sound);
                if (result == GateSystem::OpenResult::Opened) {
                    continue;
                }
            }
            if (nearbyWarp_ && nearbyWarp_.is_alive()) {
                requestWarpToStage(nearbyWarpTarget_);
                return;
            }
            cancelGuard(gameState);
            ws.systems.combatSystem.requestAttack(ws.data.player, ws.systems.sound);
            continue;
        }
        if (std::holds_alternative<StrongAttackRequested>(ev)) {
            cancelGuard(gameState);
            ws.systems.combatSystem.requestStrongAttack(ws.data.player, ws.systems.sound);
            continue;
        }
        if (const auto* look = std::get_if<MouseLookDelta>(&ev)) {
            ws.systems.cameraSystem.applyMouseLook(gameState.runtime.camera, gameState.runtime.mouseCapture,
                                                   look->xrel, look->yrel);
            continue;
        }
        if (const auto* dk = std::get_if<DebugKeyPressed>(&ev)) {
            switch (dk->scancode) {
                case SDL_SCANCODE_F6:
                    dbg.overlayCorner = nextCorner(dbg.overlayCorner);
                    std::cout << "[Debug F6] Overlay corner: " << cornerName(dbg.overlayCorner)
                              << "\n";
                    break;
                case SDL_SCANCODE_F7:
                    dbg.overlayVisible = !dbg.overlayVisible;
                    std::cout << "[Debug F7] Overlay: " << (dbg.overlayVisible ? "ON" : "OFF")
                              << "\n";
                    break;
                case SDL_SCANCODE_F8:
                    dbg.showHitboxes = !dbg.showHitboxes;
                    std::cout << "[Debug F8] Hitbox visualization: "
                              << (dbg.showHitboxes ? "ON" : "OFF") << "\n";
                    break;
                case SDL_SCANCODE_F9:
                    debugDumpAnimators(gameState);
                    break;
                case SDL_SCANCODE_F10:
                    debugLogPoolStatus(gameState);
                    break;
                case SDL_SCANCODE_F11:
                    debugClearEnemies(gameState);
                    break;
                case SDL_SCANCODE_F12:
                    debugSpawnBurst(gameState, 20);
                    break;
                default:
                    break;
            }
            continue;
        }
    }

    if (!gameOverPushed_ && ws.data.player && ws.data.player.is_alive() &&
        ws.data.player.has<CHealth>()) {
        const CHealth& hp = ws.data.player.get<CHealth>();
        if (hp.currentHp <= 0) {
            if (deathTimer_ >= kDeathDelay) {
                cmds.requestPush(factory_.createGameOverLayer());
                gameOverPushed_ = true;
            }
        } else {
            deathTimer_ = 0.f;
        }
    }
}

void GameplayLayer::updateNearbyWarpPad() {
    auto& wd = state_.worldState.data;
    if (!wd.player || !wd.player.is_alive()) {
        nearbyWarp_ = flecs::entity::null();
        return;
    }

    const glm::vec3& playerPos = wd.player.get<CTransform>().pos;
    flecs::entity bestPad = flecs::entity::null();
    float bestDistSq = std::numeric_limits<float>::max();
    StageId bestTarget = StageId::Terminal;

    wd.world.each([&](flecs::entity e, const CTransform& t, const CWarpPad& wp) {
        (void)e;
        const float dx = playerPos.x - t.pos.x;
        const float dz = playerPos.z - t.pos.z;
        const float distSq = dx * dx + dz * dz;
        if (distSq <= wp.radius * wp.radius && distSq < bestDistSq) {
            bestDistSq = distSq;
            bestPad = e;
            bestTarget = wp.targetStage;
        }
    });

    nearbyWarp_ = bestPad;
    if (nearbyWarp_) {
        nearbyWarpTarget_ = bestTarget;
        nearbyWarpTargetName_ = stage_registry::get(bestTarget).name;
    } else {
        nearbyWarpTargetName_.clear();
    }
}

void GameplayLayer::updateNearbyGate() {
    auto& wd = state_.worldState.data;
    nearbyGate_ = state_.worldState.systems.gateSystem.findNearestOpenableGate(wd);
}

void GameplayLayer::updateNearbyChest() {
    auto& wd = state_.worldState.data;
    nearbyChest_ = state_.worldState.systems.chestSystem.findNearestOpenableChest(wd);
}

void GameplayLayer::update(float dt, bool isTop, const ActionState& input) {
    debugElapsedTime_ += dt;

    if (pendingWarp_) {
        const StageId target = pendingWarpTarget_;
        pendingWarp_ = false;
        warpToStage(target);
    }

    auto& gameState = state_;
    auto& ws = gameState.worldState;

    sim_.updateEnemy(gameState, dt, gravity_);

    ws.systems.spinAnimationSystem.update(ws.data, dt);
    ws.systems.particleSystem.update(ws.data, dt);

    if (isTop) {
        sim_.updatePlayer(gameState, input, dt, gravity_, jumpSpeed_);
    } else {
        static const ActionState kZeroInput{};
        sim_.updatePlayer(gameState, kZeroInput, dt, gravity_, jumpSpeed_);
    }

    int winW = 1280, winH = 720;
    SDL_GetWindowSize(gameState.runtime.window, &winW, &winH);
    ws.systems.cameraSystem.applyViewProjectionAndLighting(
        ws.data.vulkan, gameState.runtime.camera, ws.data.player.get<CTransform>().pos, winW, winH);

    if (!gameOverPushed_ && ws.data.player && ws.data.player.is_alive() &&
        ws.data.player.has<CHealth>()) {
        if (ws.data.player.get<CHealth>().currentHp <= 0) {
            deathTimer_ += dt;
        }
    }

    updateNearbyWarpPad();
    updateNearbyGate();
    updateNearbyChest();
}

void GameplayLayer::buildScene(SceneData& scene) {
    auto& wd = state_.worldState.data;

    // ─── 1. Camera position 取得 ──────────────────
    const glm::vec3 cameraPos = wd.player.is_alive() && wd.player.has<CTransform>()
                                    ? wd.player.get<CTransform>().pos
                                    : glm::vec3{0.f};

    // ─── 2. SceneRenderer 経由で world → SceneData 構築 ──
    renderer_.buildSceneData(wd, cameraPos, scene, state_.settings.drawDistance);

    // ─── 3. CSkeletalAnim の skinMatrices を GPU buffer に転送 ──
    // SkeletalAnimSystem が sa.skinMatrices を計算済みなので、 ここで SSBO に書き込む
    const uint32_t skinFi = static_cast<uint32_t>(skinFrameIndex_);
    wd.world.each([&](flecs::entity, const CSkeletalAnim& sa) {
        if (!sa.skinSlot.valid()) return;
        if (sa.skinMatrices.empty()) return;
        wd.vulkan.skinBufferPool().update(skinFi, sa.skinSlot, sa.skinMatrices);
    });

    // ─── 4. パーティクル参照を VulkanRenderer に渡す (描画は drawFrame 内) ─
    wd.vulkan.debugLines().clear();
    wd.vulkan.setCurrentParticles(&state_.worldState.systems.particleSystem.particles());

    // ─── 5. デバッグヒットボックス (既存ロジック) ────
    if (state_.runtime.debug.showHitboxes) {
        drawAttackHitboxDebug();
        DebugLineRenderer& dl = wd.vulkan.debugLines();
        wd.world.each([&](flecs::entity e, const CTransform& et, const CEnemyAI& ai) {
            (void)e;
            drawEnemyHitboxDebug(dl, et, ai);
        });
        wd.world.each([&](flecs::entity e, const CTransform& et, const CEnemyAI& ai) {
            if (ai.isDying) return;
            const bool isSkeleton = e.has<SkeletonTag>();
            const bool isSoldier = e.has<SoldierTag>();
            if (!isSkeleton && !isSoldier) return;
            drawEnemyAttackHitboxDebug(dl, et, ai, isSkeleton);
        });
        drawPlayerHurtboxDebug(dl, wd.player, debugElapsedTime_);

        // Static obstacle collision (trees / graves / chests / rocks)
        const glm::vec4 obstacleColor{0.3f, 0.75f, 1.0f, 1.0f};  // cyan
        wd.world.each([&](flecs::entity e, const CObstacle&) {
            if (!e.has<CTransform>()) return;
            const AABB box = physics::obstacleWorldAABB(e);
            const glm::vec3 c = (box.min + box.max) * 0.5f;
            const glm::vec3 h = (box.max - box.min) * 0.5f;
            drawAABBEdges(dl, c, h, obstacleColor);
        });
    }

    // ─── 6. HUD ── (clear は VulkanRenderer がフレーム境界で行う。 ここは積むだけ)
    if (wd.player && wd.player.is_alive()) {
        const float gripMargin = 12.f;
        const float gripRadius = hud_system::gripGaugeRadius();
        const float gripCenterX = gripMargin + gripRadius;
        const float gripCenterY = 104.f;
        if (wd.player.has<CGrip>()) {
            const CGrip& grip = wd.player.get<CGrip>();
            hud_system::drawGripGauge(wd.vulkan.hud(), grip, gripCenterX, gripCenterY);
        }
        const float barOriginX = gripCenterX + gripRadius + 12.f;
        if (wd.player.has<CHealth>()) {
            const CHealth& hp = wd.player.get<CHealth>();
            hud_system::drawHealthBar(wd.vulkan.hud(), hp, barOriginX, 80.f);
        }
        if (wd.player.has<CShield>()) {
            const CShield& sh = wd.player.get<CShield>();
            hud_system::drawShieldGauge(wd.vulkan.hud(), sh, barOriginX, 110.f);
        }
    }

    // ─── 7. skinFrameIndex 進める ─────────────────
    skinFrameIndex_ = (skinFrameIndex_ + 1) % FrameSync::MAX_FRAMES_IN_FLIGHT;
}

void GameplayLayer::drawAttackHitboxDebug() {
    auto& wd = state_.worldState.data;
    if (!wd.player || !wd.player.is_alive()) return;
    if (!wd.player.has<CAttack>()) return;

    const CAttack& atk = wd.player.get<CAttack>();
    if (!atk.isActive() || !atk.def) return;

    const AttackDef& def = *atk.def;
    const CTransform& pt = wd.player.get<CTransform>();

    const glm::vec3 origin{pt.pos.x, pt.pos.y + pt.scale.y * 0.5f, pt.pos.z};

    DebugLineRenderer& dl = wd.vulkan.debugLines();

    const float yawRad = glm::radians(pt.yaw);
    const float cosYaw = std::cos(yawRad);
    const float sinYaw = std::sin(yawRad);
    auto localToWorld = [&](const glm::vec3& v) -> glm::vec3 {
        return glm::vec3{v.x * cosYaw + v.z * sinYaw, v.y, -v.x * sinYaw + v.z * cosYaw};
    };

    const glm::vec3 startDirWMidFull = localToWorld(def.startDir);
    const glm::vec3 axisW = localToWorld(def.rotationAxis);

    const glm::vec3 startDirLocalPosFull =
        tiltStartDir(def.startDir, def.rotationAxis, +def.halfWidthDeg);
    const glm::vec3 startDirLocalNegFull =
        tiltStartDir(def.startDir, def.rotationAxis, -def.halfWidthDeg);
    const glm::vec3 startDirWPosFull = localToWorld(startDirLocalPosFull);
    const glm::vec3 startDirWNegFull = localToWorld(startDirLocalNegFull);

    const glm::vec4 kFillColor{1.f, 0.f, 0.f, 0.30f};
    const glm::vec4 kFillSideColor{1.f, 0.2f, 0.2f, 0.15f};
    const glm::vec4 kOutlineColor{1.f, 0.3f, 0.3f, 1.0f};
    const glm::vec4 kOutlineSideColor{1.f, 0.4f, 0.4f, 0.6f};
    const glm::vec4 kBladeColor{1.f, 1.f, 0.f, 1.0f};
    const glm::vec4 kRangeArcColor{1.f, 0.3f, 0.3f, 0.25f};
    const glm::vec4 kRangeArcSideCol{1.f, 0.4f, 0.4f, 0.15f};

    const int fullSegs = std::max(16, static_cast<int>(std::abs(def.sweepAngleDeg) * 0.3f));
    dl.addSectorOutlineAxis(origin, startDirWMidFull, axisW, def.sweepAngleDeg, def.range,
                            kRangeArcColor, fullSegs);
    dl.addSectorOutlineAxis(origin, startDirWPosFull, axisW, def.sweepAngleDeg, def.range,
                            kRangeArcSideCol, fullSegs);
    dl.addSectorOutlineAxis(origin, startDirWNegFull, axisW, def.sweepAngleDeg, def.range,
                            kRangeArcSideCol, fullSegs);

    const float activeStart = def.activeStart();
    const float activeEnd = def.activeEnd();
    const bool inActive = (atk.elapsed >= activeStart && atk.elapsed < activeEnd);
    if (!inActive) return;

    if (glm::length(atk.prevSweepWorldDir) < 0.01f) return;

    const glm::vec3 prevWorldDir = atk.prevSweepWorldDir;
    const glm::vec3 currWorldDir = getCurrentSweepWorldDirForDraw(atk, pt);

    const glm::vec3 prevDirSidePos = tiltStartDir(prevWorldDir, axisW, +def.halfWidthDeg);
    const glm::vec3 prevDirSideNeg = tiltStartDir(prevWorldDir, axisW, -def.halfWidthDeg);

    const float dotPC = glm::clamp(glm::dot(prevWorldDir, currWorldDir), -1.f, 1.f);
    const float frameAngleAbs = glm::degrees(std::acos(dotPC));
    const float frameSweep = (def.sweepAngleDeg >= 0.f) ? frameAngleAbs : -frameAngleAbs;

    const int frameSegs = std::max(2, static_cast<int>(frameAngleAbs * 0.6f));

    dl.addSectorFilledAxis(origin, prevWorldDir, axisW, frameSweep, def.range, kFillColor,
                           frameSegs);
    dl.addSectorOutlineAxis(origin, prevWorldDir, axisW, frameSweep, def.range, kOutlineColor,
                            frameSegs);

    dl.addSectorFilledAxis(origin, prevDirSidePos, axisW, frameSweep, def.range, kFillSideColor,
                           frameSegs);
    dl.addSectorOutlineAxis(origin, prevDirSidePos, axisW, frameSweep, def.range, kOutlineSideColor,
                            frameSegs);
    dl.addSectorFilledAxis(origin, prevDirSideNeg, axisW, frameSweep, def.range, kFillSideColor,
                           frameSegs);
    dl.addSectorOutlineAxis(origin, prevDirSideNeg, axisW, frameSweep, def.range, kOutlineSideColor,
                            frameSegs);

    const glm::vec3 bladeTip = origin + currWorldDir * def.range;
    dl.addLine(origin, bladeTip, kBladeColor);
}

glm::vec3 GameplayLayer::getCurrentSweepWorldDirForDraw(const CAttack& atk,
                                                        const CTransform& at) const {
    const AttackDef& def = *atk.def;
    const float t = glm::clamp((atk.elapsed - def.activeStart()) / def.activeTime, 0.f, 1.f);
    const glm::vec3 localDir = def.dirAt(t);

    const float yawRad = glm::radians(at.yaw);
    const float cosYaw = std::cos(yawRad);
    const float sinYaw = std::sin(yawRad);
    return glm::vec3{localDir.x * cosYaw + localDir.z * sinYaw, localDir.y, -localDir.x * sinYaw + localDir.z * cosYaw};
}

void GameplayLayer::drawImGui() {
    if (state_.runtime.debug.overlayVisible) {
        state_.worldState.systems.renderDebugSystem.draw(buildDebugOverlayData(state_),
                                                         state_.runtime.debug.overlayCorner);
    }

    {
        const StageDef& def = stage_registry::get(currentStage_);
        const float viewportW = ImGui::GetMainViewport()->Size.x;
        ImGui::SetNextWindowPos(ImVec2(viewportW * 0.5f, 24.f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##StageName", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 0.9f, 0.9f), "[%s]", def.name.c_str());
        ImGui::End();
    }

    // ヒント表示: Chest > Gate > Warp
    std::string hint;
    ImVec4 hintColor{1.f, 0.85f, 0.3f, 1.f};
    if (nearbyChest_ && nearbyChest_.is_alive() && nearbyChest_.has<CChest>()) {
        const CChest& c = nearbyChest_.get<CChest>();
        if (playerCanOpenChest(state_.worldState.data.player, nearbyChest_)) {
            const KeyMapping& m = state_.settings.keyMapping;
            hint = "Press " + m.attack.shortName() + " to open Chest";
        } else {
            hint = std::string("Need ") + keyTypeName(c.requiredKey) + " Key (Chest)";
            hintColor = ImVec4(1.f, 0.4f, 0.4f, 1.f);
        }
    } else if (nearbyGate_ && nearbyGate_.is_alive() && nearbyGate_.has<CGate>()) {
        const CGate& g = nearbyGate_.get<CGate>();
        if (playerCanOpenGate(state_.worldState.data.player, nearbyGate_)) {
            const KeyMapping& m = state_.settings.keyMapping;
            hint = "Press " + m.attack.shortName() + " to open Gate";
        } else {
            hint = std::string("Need ") + keyTypeName(g.requiredKey) + " Key";
            hintColor = ImVec4(1.f, 0.4f, 0.4f, 1.f);
        }
    } else if (nearbyWarp_ && nearbyWarp_.is_alive() && !nearbyWarpTargetName_.empty()) {
        const KeyMapping& m = state_.settings.keyMapping;
        hint = "Press " + m.attack.shortName() + " to warp to [" + nearbyWarpTargetName_ + "]";
    }

    if (!hint.empty()) {
        const ImVec2 viewport = ImGui::GetMainViewport()->Size;
        ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.75f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f, 8.f));
        ImGui::Begin("##InteractHint", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
        ImGui::SetWindowFontScale(1.4f);
        ImGui::TextColored(hintColor, "%s", hint.c_str());
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

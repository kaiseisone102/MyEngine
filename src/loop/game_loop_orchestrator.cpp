// src/loop/game_loop_orchestrator.cpp
// =============================================================================
// Phase 5-B 段階B-C: appendAttachments + 最初の3フレームだけログ
// =============================================================================
#define NOMINMAX
#include "loop/game_loop_orchestrator.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/components.h"
#include "core/spawn_trigger.h"
#include "renderer/animation.h"
#include "renderer/animator.h"
#include "renderer/skin_buffer_pool.h"
#include "systems/render_debug_system.h"
#include "systems/simulation_system.h"
#include "systems/spawn_system.h"

namespace {

void applyMouseCapture(SDL_Window* window, bool capture, bool& mouseCapture) {
    mouseCapture = capture;
    SDL_SetWindowRelativeMouseMode(window, capture);
    if (capture)
        SDL_HideCursor();
    else
        SDL_ShowCursor();
}

// =============================================================================
// Phase 5-B 段階B-C: CEquipment attach 描画
// =============================================================================
// 計算式 (修正版):
//   finalMat = entityWorld * boneWorld * localOffset * scale(1.0)
//
//   - boneWorld には rootNode->mTransformation の 0.01 倍が既に含まれる
//   - 装備モデルは cm 単位だが、 boneWorld の 0.01 倍で正しい m 単位に変換
//   - 追加スケールは不要 (1.0)
void appendAttachments(GameState& s) {
    auto& vulkan = s.worldState.data.vulkan;
    auto& scene = vulkan.scene();

    static int s_frameCount = 0;
    const bool doLog = (s_frameCount < 2);
    if (doLog) {
        std::printf("\n[Attach] === Frame %d ===\n", s_frameCount);
        s_frameCount++;
    }

    s.worldState.data.world.each([&](flecs::entity e, const CTransform& t, const CSkeletalAnim& sa,
                                     const CEquipment& eq) {
        (void)e;
        if (!sa.animator.ready()) return;

        const glm::mat4 entityWorld = t.matrix();

        if (eq.hasLeftEquip()) {
            const glm::mat4 boneWorld = sa.animator.boneWorldTransform(eq.leftHandBoneIdx);
            const glm::mat4 scaleMat = glm::scale(glm::mat4(1.f), eq.leftHandScale);
            const glm::mat4 finalMat = entityWorld * boneWorld * eq.leftHandLocalOffset * scaleMat;
            scene.addStaticModelObject(finalMat, eq.leftHandModel);

            if (doLog) {
                std::printf(
                    "  LEFT  finalMat pos = (%.3f, %.3f, %.3f) "
                    "scale_diag=(%.4f, %.4f, %.4f)\n",
                    finalMat[3].x, finalMat[3].y, finalMat[3].z, finalMat[0].x, finalMat[1].y,
                    finalMat[2].z);
            }
        }

        if (eq.hasRightEquip()) {
            const glm::mat4 boneWorld = sa.animator.boneWorldTransform(eq.rightHandBoneIdx);
            const glm::mat4 scaleMat = glm::scale(glm::mat4(1.f), eq.rightHandScale);
            const glm::mat4 finalMat = entityWorld * boneWorld * eq.rightHandLocalOffset * scaleMat;
            scene.addStaticModelObject(finalMat, eq.rightHandModel);

            if (doLog) {
                std::printf(
                    "  RIGHT finalMat pos = (%.3f, %.3f, %.3f) "
                    "scale_diag=(%.4f, %.4f, %.4f)\n",
                    finalMat[3].x, finalMat[3].y, finalMat[3].z, finalMat[0].x, finalMat[1].y,
                    finalMat[2].z);
            }
        }
    });
}

void buildDrawList(GameState& s, uint32_t skinFrameIndex) {
    auto& vulkan = s.worldState.data.vulkan;
    auto& scene = vulkan.scene();
    scene.clearObjects();

    s.worldState.data.world.each([&](flecs::entity e, const CTransform& t, CSkeletalAnim& sa) {
        (void)e;
        if (!sa.skinSlot.valid()) return;
        if (!sa.skinMatrices.empty()) {
            vulkan.skinBufferPool().update(skinFrameIndex, sa.skinSlot, sa.skinMatrices);
        }
        scene.addModelObject(t.matrix(), static_cast<int32_t>(sa.skinSlot.boneOffset), sa.model);
    });
    scene.sortModelDrawListBySourceModel();

    s.worldState.data.world.each(
        [&](flecs::entity e, const CTransform& t, const CStaticModelRef& sm) {
            (void)e;
            if (!sm.sourceModel) return;
            scene.addStaticModelObject(t.matrix(), sm.sourceModel);
        });

    appendAttachments(s);

    scene.sortStaticModelDrawListBySourceModel();

    s.worldState.data.world.each([&](flecs::entity e, const CTransform& t) {
        if (e.has<CSkeletalAnim>()) return;
        if (e.has<CStaticModelRef>()) return;
        scene.addMeshObject(t.matrix());
    });
}

DebugOverlayData buildDebugOverlayData(GameState& s) {
    int entityCount = 0;
    s.worldState.data.world.each([&](const CTransform&) { ++entityCount; });

    const CHealth& hp = s.worldState.data.player.get<CHealth>();
    const CAttack& atk = s.worldState.data.player.get<CAttack>();
    const CShield& sh = s.worldState.data.player.has<CShield>()
                            ? s.worldState.data.player.get<CShield>()
                            : CShield{};

    const SkinBufferPool& pool = s.worldState.data.vulkan.skinBufferPool();

    return {entityCount,
            s.runtime.fps,
            s.worldState.data.player.get<CTransform>().pos,
            s.worldState.data.player.get<CVelocity>().y,
            s.worldState.data.player.get<CPhysics>().onGround,
            s.runtime.camera.mode == CameraMode::TPS,
            atk.active,
            atk.timer,
            hp.segmentCount,
            hp.unlockedSegments,
            hp.currentHp,
            hp.invincTimer,
            sh.type,
            sh.durability,
            static_cast<int>(pool.allocatedCount()),
            static_cast<int>(SkinBufferPool::MAX_ENTITIES)};
}

void debugSpawnBurst(GameState& s, int count) {
    auto& wd = s.worldState.data;
    const glm::vec3& playerPos = wd.player.get<CTransform>().pos;

    int spawned = 0;
    int failed = 0;
    static int s_burstIndex = 0;

    for (int i = 0; i < count; ++i) {
        const float angle = static_cast<float>(rand()) / RAND_MAX * 6.28318f;
        const float dist = 4.f + static_cast<float>(rand()) / RAND_MAX * 4.f;
        const glm::vec3 spawnPos{playerPos.x + std::cos(angle) * dist, 0.5f,
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
              << SkinBufferPool::MAX_ENTITIES << ")\n";
}

void debugClearEnemies(GameState& s) {
    auto& wd = s.worldState.data;
    const size_t before = wd.enemies.size();
    for (flecs::entity e : wd.enemies) {
        if (e.is_alive()) e.destruct();
    }
    wd.enemies.clear();
    std::cout << "[Debug F11] Cleared " << before
              << " enemies (Pool: " << wd.vulkan.skinBufferPool().allocatedCount() << "/"
              << SkinBufferPool::MAX_ENTITIES << ")\n";
}

void debugLogPoolStatus(GameState& s) {
    const SkinBufferPool& pool = s.worldState.data.vulkan.skinBufferPool();
    int entityCount = 0;
    s.worldState.data.world.each([&](const CTransform&) { ++entityCount; });

    std::cout << "[Debug F10] SkinBufferPool: " << pool.allocatedCount() << " / "
              << SkinBufferPool::MAX_ENTITIES << " | Entities: " << entityCount
              << " | Enemies: " << s.worldState.data.enemies.size() << " | FPS: " << s.runtime.fps
              << "\n";
}

void debugDumpAnimators(GameState& s) {
    std::cout << "\n[Debug F9] === Animator State Dump ===\n";
    std::cout << "  entity                animator         clip             "
              << "clipName        skeleton         slot  matricesPtr      state(prev->cur)\n";

    int count = 0;
    s.worldState.data.world.each([&](flecs::entity e, CSkeletalAnim& sa) {
        const char* name = e.name().c_str();
        if (!name || name[0] == '\0') name = "(unnamed)";

        const AnimationClip* clip = sa.animator.currentClip();
        const char* clipName = (clip && !clip->name.empty()) ? clip->name.c_str() : "(null)";
        const void* clipPtr = clip;
        const void* matricesPtr =
            sa.skinMatrices.empty() ? nullptr : static_cast<const void*>(sa.skinMatrices.data());

        const void* skeletonPtr =
            sa.model ? static_cast<const void*>(&sa.model->skeleton()) : nullptr;

        const char* curName = "?";
        const char* prevName = "?";
        if (e.has<CAnimState>()) {
            const CAnimState& as = e.get<CAnimState>();
            curName = animStateName(as.current);
            prevName = animStateName(as.previous);
        }

        std::printf("  %-20s  %p  %p  %-14s  %p  %4u  %p  %s -> %s\n", name,
                    static_cast<const void*>(&sa.animator), clipPtr, clipName, skeletonPtr,
                    sa.skinSlot.boneOffset, matricesPtr, prevName, curName);
        count++;
    });

    std::cout << "[Debug F9] Total: " << count << " entities with CSkeletalAnim\n\n";
}

}  // namespace

void GameLoopOrchestrator::run(GameState& s, float gravity, float jumpSpeed) const {
    SimulationSystem sim;
    bool running = true;

    uint32_t skinFrameIndex = 0;

    bool prevF9 = false, prevF10 = false, prevF11 = false, prevF12 = false;

    while (running) {
        const uint64_t now = SDL_GetTicks();
        float dt = std::min(static_cast<float>(now - s.runtime.lastTicks) / 1000.f, 0.05f);
        s.runtime.lastTicks = now;
        s.runtime.fpsFrames += 1;
        s.runtime.fpsAccum += dt;
        if (s.runtime.fpsAccum >= 0.5f) {
            s.runtime.fps = static_cast<float>(s.runtime.fpsFrames) / s.runtime.fpsAccum;
            s.runtime.fpsAccum = 0.f;
            s.runtime.fpsFrames = 0;
        }

        s.worldState.systems.inputSystem.collectEvents(s.runtime.window, s.runtime.mouseCapture,
                                                       s.worldState.systems.eventBus);
        s.worldState.systems.eventConsumerSystem.consume(
            s.worldState.systems.eventBus, running, s.runtime.mouseCapture, s.runtime.camera,
            s.worldState.systems.cameraSystem, s.worldState.systems.combatSystem,
            s.worldState.systems.audioEventSystem, s.worldState.data.player,
            s.worldState.systems.sound, s.worldState.data.vulkan, [&](bool capture) {
                applyMouseCapture(s.runtime.window, capture, s.runtime.mouseCapture);
            });

        const bool* keys = SDL_GetKeyboardState(nullptr);

        const bool nowF9 = keys[SDL_SCANCODE_F9];
        const bool nowF10 = keys[SDL_SCANCODE_F10];
        const bool nowF11 = keys[SDL_SCANCODE_F11];
        const bool nowF12 = keys[SDL_SCANCODE_F12];
        if (nowF9 && !prevF9) debugDumpAnimators(s);
        if (nowF10 && !prevF10) debugLogPoolStatus(s);
        if (nowF11 && !prevF11) debugClearEnemies(s);
        if (nowF12 && !prevF12) debugSpawnBurst(s, 20);
        prevF9 = nowF9;
        prevF10 = nowF10;
        prevF11 = nowF11;
        prevF12 = nowF12;

        sim.updateEnemy(s, dt, gravity);
        sim.updatePlayer(s, keys, dt, gravity, jumpSpeed);

        int winW = 1280, winH = 720;
        SDL_GetWindowSize(s.runtime.window, &winW, &winH);
        s.worldState.systems.cameraSystem.applyViewProjectionAndLighting(
            s.worldState.data.vulkan, s.runtime.camera,
            s.worldState.data.player.get<CTransform>().pos, winW, winH);

        buildDrawList(s, skinFrameIndex);
        s.worldState.data.vulkan.drawFrame(
            [&]() { s.worldState.systems.renderDebugSystem.draw(buildDebugOverlayData(s)); });

        skinFrameIndex = (skinFrameIndex + 1) % FrameSync::MAX_FRAMES_IN_FLIGHT;
    }
}

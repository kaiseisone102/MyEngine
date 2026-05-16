// =============================================================================
// scene_builder_from_world.cpp — + chest alpha フェード
// =============================================================================
// CChest を持つエンティティは、 State::Open になってから openElapsed 経過時間に
// 応じて alpha を計算:
//   0.0〜1.0 秒: alpha = 1.0 (待機)
//   1.0〜3.0 秒: alpha = 1.0 → 0.0 (フェードアウト)
//   3.0 秒以上: 削除済み (chest_system 側で destruct)
// =============================================================================
#define NOMINMAX
#include "world/scene_builder_from_world.h"

#include <SDL3/SDL.h>

#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/chest.h"
#include "core/components.h"
#include "core/game_state.h"
#include "core/grave.h"
#include "renderer/animator.h"
#include "renderer/material.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/terrain_mesh.h"
#include "scene/renderable.h"
#include "scene/scene.h"

namespace {

// chest 用 alpha 計算。 開いてからの経過時間で 0〜1 を返す。
float computeChestAlpha(const CChest& c) {
    if (c.state != CChest::State::Open) return 1.0f;
    if (c.openElapsed < CChest::kFadeStartTime) return 1.0f;

    const float t = c.openElapsed - CChest::kFadeStartTime;
    const float fade = std::clamp(t / CChest::kFadeDuration, 0.0f, 1.0f);
    return 1.0f - fade;
}

}  // namespace

namespace world_scene {

void buildSceneFromWorld(scene::Scene& scene, GameState& state, uint32_t skinFrameIndex) {
    auto& wd = state.worldState.data;
    auto& vulkan = wd.vulkan;

    // ─── カメラ ──────────────────────────────────────────────────
    int winW = 1280, winH = 720;
    SDL_GetWindowSize(state.runtime.window, &winW, &winH);
    const float aspect = (winH > 0) ? static_cast<float>(winW) / static_cast<float>(winH) : 1.f;
    const glm::vec3& playerPos = wd.player.get<CTransform>().pos;

    scene.setCameraView(state.runtime.camera.getViewMatrix(playerPos));
    scene.setCameraProjection(state.runtime.camera.getProjectionMatrix(aspect));
    scene.setCameraWorldPosition(state.runtime.camera.getCameraWorldPosition(playerPos));

    // ─── ライト ──────────────────────────────────────────────────
    scene.setLightTarget(playerPos);
    scene.setLightOffset({8.f, 15.f, 8.f});
    scene.setLightColor({1.f, 1.f, 1.f});

    scene::EnvSettings env;
    env.ambient = 0.15f;
    env.specular = 0.5f;
    env.shadowStrength = 0.8f;
    env.shadowBias = 0.0015f;
    scene.setEnv(env);

    // ─── Terrain ─────────────────────────────────────────────────
    for (const auto& mesh : wd.terrains.meshes()) {
        if (!mesh) continue;
        auto* node = scene.addRoot();
        node->setOverrideMatrix(glm::mat4(1.f));
        node->setRenderable<scene::TerrainRenderable>(mesh.get(), mesh->material());
    }

    // ─── Skinned entity ──────────────────────────────────────────
    wd.world.each([&](flecs::entity e, const CTransform& t, CSkeletalAnim& sa) {
        (void)e;
        if (!sa.skinSlot.valid()) return;

        if (!sa.skinMatrices.empty()) {
            vulkan.skinBufferPool().update(skinFrameIndex, sa.skinSlot, sa.skinMatrices);
        }

        auto* node = scene.addRoot();
        node->setOverrideMatrix(t.matrix());
        node->setRenderable<scene::SkinnedMeshRenderable>(sa.model,
                                                          static_cast<int>(sa.skinSlot.boneOffset));
    });

    // ─── CStaticModelRef ─────────────────────────────────────────
    // CChest がある場合は chest 用 alpha 計算
    wd.world.each([&](flecs::entity e, const CTransform& t, const CStaticModelRef& sm) {
        if (!sm.sourceModel) return;
        auto* node = scene.addRoot();
        node->setOverrideMatrix(t.matrix());
        auto* renderable = node->setRenderable<scene::StaticMeshRenderable>(sm.sourceModel);

        if (e.has<CChest>()) {
            const CChest& c = e.get<CChest>();
            renderable->setAlpha(computeChestAlpha(c));
        }

        if (e.has<CGrave>()) {
            const CGrave& g = e.get<CGrave>();
            if (g.state == CGrave::State::Destroyed) {
                const float elapsed = g.destroyedElapsed;
                if (elapsed >= CGrave::kFadeStartTime) {
                    const float fadeT = (elapsed - CGrave::kFadeStartTime) / CGrave::kFadeDuration;
                    alpha = 1.0f - glm::clamp(fadeT, 0.f, 1.f);
                }
            }
        }
    });

    // ─── 装備 attach ─────────────────────────────────────────────
    wd.world.each([&](flecs::entity e, const CTransform& t, const CSkeletalAnim& sa,
                      const CEquipment& eq) {
        (void)e;
        if (!sa.animator.ready()) return;

        const glm::mat4 entityWorld = t.matrix();

        if (eq.hasLeftEquip()) {
            const glm::mat4 boneWorld = sa.animator.boneWorldTransform(eq.leftHandBoneIdx);
            const glm::mat4 scaleMat = glm::scale(glm::mat4(1.f), eq.leftHandScale);
            const glm::mat4 finalMat = entityWorld * boneWorld * eq.leftHandLocalOffset * scaleMat;
            auto* node = scene.addRoot();
            node->setOverrideMatrix(finalMat);
            node->setRenderable<scene::StaticMeshRenderable>(eq.leftHandModel);
        }

        if (eq.hasRightEquip()) {
            const glm::mat4 boneWorld = sa.animator.boneWorldTransform(eq.rightHandBoneIdx);
            const glm::mat4 scaleMat = glm::scale(glm::mat4(1.f), eq.rightHandScale);
            const glm::mat4 finalMat = entityWorld * boneWorld * eq.rightHandLocalOffset * scaleMat;
            auto* node = scene.addRoot();
            node->setOverrideMatrix(finalMat);
            node->setRenderable<scene::StaticMeshRenderable>(eq.rightHandModel);
        }
    });

    // ─── Cube ────────────────────────────────────────────────────
    wd.world.each([&](flecs::entity e, const CTransform& t) {
        if (e.has<CSkeletalAnim>()) return;
        if (e.has<CStaticModelRef>()) return;
        auto* node = scene.addRoot();
        node->setOverrideMatrix(t.matrix());
        const Material* mat = nullptr;
        if (e.has<CMaterialRef>()) {
            mat = e.get<CMaterialRef>().material;
        }
        node->setRenderable<scene::CubeRenderable>(mat);
    });
}

}  // namespace world_scene

// =============================================================================
// scene_renderer.cpp — Phase 1C: WorldData → SceneData
// =============================================================================
// 手元 scene_data.h 整合版:
//   - addXxxObject は存在しない → 直接 listOpaque/listTransparent に push_back
//   - sortXxxBySourceModel は存在しない → std::sort で自前ソート
//   - SkinBufferPool::Slot::offset() は存在しない → .boneOffset メンバアクセス
// =============================================================================
#define NOMINMAX
#include "scene/scene_renderer.h"

#include <algorithm>
#include <cmath>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/chest.h"
#include "core/components.h"
#include "core/game_state.h"
#include "core/grave.h"
#include "core/water.h"
#include "renderer/animator.h"
#include "renderer/material.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/terrain_mesh.h"
#include "renderer/water_mesh.h"
#include "scene/scene_data.h"

namespace {

constexpr float kOpaqueThreshold = 0.999f;

// chest 用 alpha フェード計算
float computeChestAlpha(const CChest& c) {
    if (c.state != CChest::State::Open) return 1.0f;
    if (c.openElapsed < CChest::kFadeStartTime) return 1.0f;
    const float t = c.openElapsed - CChest::kFadeStartTime;
    const float fade = std::clamp(t / CChest::kFadeDuration, 0.0f, 1.0f);
    return 1.0f - fade;
}

// grave 用 alpha フェード計算
float computeGraveAlpha(const CGrave& g) {
    if (g.state != CGrave::State::Destroyed) return 1.0f;
    if (g.destroyedElapsed < CGrave::kFadeStartTime) return 1.0f;
    const float t = g.destroyedElapsed - CGrave::kFadeStartTime;
    const float fade = std::clamp(t / CGrave::kFadeDuration, 0.0f, 1.0f);
    return 1.0f - fade;
}

}  // namespace

void SceneRenderer::buildSceneData(const WorldData& wd, const glm::vec3& cameraPos,
                                     SceneData& out, float cullingDistance) {
    out.clear();
    out.setCullingDistance(cullingDistance);

    const bool cullEnabled = cullingDistance > 0.f;
    const float cullSq = cullEnabled ? cullingDistance * cullingDistance : 0.f;

    auto inRange = [&](const glm::vec3& pos) -> bool {
        if (!cullEnabled) return true;
        const glm::vec3 d = pos - cameraPos;
        return glm::dot(d, d) <= cullSq;
    };

    // alpha で opaque/transparent を振り分ける helper lambdas
    auto addMesh = [&](const glm::mat4& m, const Material* mat, float alpha) {
        if (alpha >= kOpaqueThreshold) {
            out.meshDrawListOpaque().push_back({m, mat, 1.0f});
        } else {
            out.meshDrawListTransparent().push_back({m, mat, alpha});
        }
    };
    auto addStaticModel = [&](const glm::mat4& m, const Model* model, float alpha) {
        if (alpha >= kOpaqueThreshold) {
            out.staticModelDrawListOpaque().push_back({m, model, 1.0f});
        } else {
            out.staticModelDrawListTransparent().push_back({m, model, alpha});
        }
    };
    auto addSkinned = [&](const glm::mat4& m, const Model* model, int skinOffset, float alpha) {
        if (alpha >= kOpaqueThreshold) {
            out.modelDrawListOpaque().push_back({m, model, skinOffset, 1.0f});
        } else {
            out.modelDrawListTransparent().push_back({m, model, skinOffset, alpha});
        }
    };
    auto addTerrain = [&](const glm::mat4& m, const TerrainMesh* terrain,
                            const Material* mat, float alpha) {
        if (alpha >= kOpaqueThreshold) {
            out.terrainDrawListOpaque().push_back({m, terrain, mat, 1.0f});
        } else {
            out.terrainDrawListTransparent().push_back({m, terrain, mat, alpha});
        }
    };

    // ─── 1. CSkeletalAnim → SkinnedDrawItem ──────────────────
    wd.world.each([&](flecs::entity, const CTransform& t, const CSkeletalAnim& sa) {
        if (!sa.model) return;
        if (!inRange(t.pos)) return;
        addSkinned(t.matrix(), sa.model,
                     static_cast<int>(sa.skinSlot.boneOffset),  // ← メンバアクセス
                     1.0f);
    });

    // ─── 2. CStaticModelRef → StaticModelDrawItem (chest/grave で alpha) ─
    wd.world.each([&](flecs::entity e, const CTransform& t, const CStaticModelRef& sm) {
        if (!sm.sourceModel) return;
        if (!inRange(t.pos)) return;

        float alpha = 1.0f;
        if (e.has<CChest>()) {
            alpha = computeChestAlpha(e.get<CChest>());
        } else if (e.has<CGrave>()) {
            alpha = computeGraveAlpha(e.get<CGrave>());
        }
        addStaticModel(t.matrix(), sm.sourceModel, alpha);
    });

    // ─── 3. 装備品 attach (CSkeletalAnim + CEquipment) ───────
    wd.world.each([&](flecs::entity, const CTransform& t, const CSkeletalAnim& sa,
                       const CEquipment& eq) {
        if (!sa.animator.ready()) return;
        if (!inRange(t.pos)) return;

        const glm::mat4 entityWorld = t.matrix();

        if (eq.hasLeftEquip()) {
            const glm::mat4 boneWorld = sa.animator.boneWorldTransform(eq.leftHandBoneIdx);
            const glm::mat4 scaleMat = glm::scale(glm::mat4(1.f), eq.leftHandScale);
            const glm::mat4 finalMat =
                entityWorld * boneWorld * eq.leftHandLocalOffset * scaleMat;
            addStaticModel(finalMat, eq.leftHandModel, 1.0f);
        }

        if (eq.hasRightEquip()) {
            const glm::mat4 boneWorld = sa.animator.boneWorldTransform(eq.rightHandBoneIdx);
            const glm::mat4 scaleMat = glm::scale(glm::mat4(1.f), eq.rightHandScale);
            const glm::mat4 finalMat =
                entityWorld * boneWorld * eq.rightHandLocalOffset * scaleMat;
            addStaticModel(finalMat, eq.rightHandModel, 1.0f);
        }
    });

    // ─── 4. Cube Mesh ────────────────────────────────────────
    wd.world.each([&](flecs::entity e, const CTransform& t) {
        if (e.has<CSkeletalAnim>()) return;
        if (e.has<CStaticModelRef>()) return;
        if (e.has<CWater>()) return;
        if (!inRange(t.pos)) return;

        const Material* mat = nullptr;
        if (e.has<CMaterialRef>()) {
            mat = e.get<CMaterialRef>().material;
        }
        addMesh(t.matrix(), mat, 1.0f);
    });

    // ─── 5. Terrain ──────────────────────────────────────────
    for (const auto& mesh : wd.terrains.meshes()) {
        if (!mesh) continue;
        if (cullEnabled) {
            const glm::vec3 center = mesh->worldCenter();
            const float radius = mesh->boundingRadius();
            const float d = glm::length(center - cameraPos);
            if (d - radius > cullingDistance) continue;
        }
        addTerrain(glm::mat4(1.f), mesh.get(), mesh->material(), 1.0f);
    }

    // ─── 6. Water ────────────────────────────────────────────
    wd.world.each([&](flecs::entity, const CWater& w) {
        if (!w.mesh) return;
        WaterDrawItem item;
        item.center = w.center;
        item.sizeXZ = w.sizeXZ;
        item.mesh = w.mesh;
        item.drawParams = w.drawParams;
        out.waterDrawList().push_back(item);
    });

    // ─── 7. Opaque ソート (sourceModel グループ化、 texture switch 削減) ─
    std::sort(out.modelDrawListOpaque().begin(), out.modelDrawListOpaque().end(),
                [](const SkinnedDrawItem& a, const SkinnedDrawItem& b) {
                    return a.sourceModel < b.sourceModel;
                });
    std::sort(out.staticModelDrawListOpaque().begin(), out.staticModelDrawListOpaque().end(),
                [](const StaticModelDrawItem& a, const StaticModelDrawItem& b) {
                    return a.sourceModel < b.sourceModel;
                });

    // ─── 8. Transparent ソート (奥→手前、 alpha blend 正しさのため) ─
    auto sortByCameraDistDesc = [&cameraPos](auto& list, auto getPos) {
        std::sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
            const glm::vec3 pa = getPos(a);
            const glm::vec3 pb = getPos(b);
            const float da = glm::dot(pa - cameraPos, pa - cameraPos);
            const float db = glm::dot(pb - cameraPos, pb - cameraPos);
            return da > db;
        });
    };

    sortByCameraDistDesc(out.meshDrawListTransparent(),
                          [](const MeshDrawItem& i) { return glm::vec3(i.model[3]); });
    sortByCameraDistDesc(out.staticModelDrawListTransparent(),
                          [](const StaticModelDrawItem& i) { return glm::vec3(i.model[3]); });
    sortByCameraDistDesc(out.modelDrawListTransparent(),
                          [](const SkinnedDrawItem& i) { return glm::vec3(i.model[3]); });
    sortByCameraDistDesc(out.terrainDrawListTransparent(),
                          [](const TerrainDrawItem& i) {
                              return i.terrain ? i.terrain->worldCenter() : glm::vec3(0.f);
                          });
}

// include/MyEngine/renderer/static_cull_build.h
#pragma once
// =============================================================================
// static_cull_build.h - Phase 2B PART3c (3c-1): unified static draw builder.
// =============================================================================
// Walks main's OPAQUE static draws (cube mesh + static-model submeshes + terrain)
// ONCE, in encounter order, assigning a contiguous drawId per draw. For each
// drawId it emits aligned records that share that id:
//   - DrawData slot     (pool.pushOne)  -> read by the vertex shader via gl_InstanceIndex
//   - CullObject        (world sphere, drawId) -> cull.comp writes cmds[drawId].instanceCount
//   - DrawTemplate      (indexCount/firstIndex/vertexOffset, firstInstance = DrawData slot)
//   - PreparedDraw      (blockIndex + draw range) -> CPU draw now, MDI in 3c-2
// Because pushOne returns the ABSOLUTE slot, the slot already accounts for the
// reflection draws pushed earlier in the same frame (no Nrefl bookkeeping).
//
// SCOPE: main opaque static only. Transparent static and ALL reflection static
// stay on the PART3b CPU-loop (static_draw.h); transparent is not culled and
// reflection uses a different frustum. Header-only (inline): no new TU.
// =============================================================================
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

#include <glm/glm.hpp>

#include "core/aabb.h"
#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/material.h"
#include "renderer/terrain_mesh.h"
#include "renderer/draw_data_pool.h"
#include "renderer/culling_pass.h"
#include "scene/scene_data.h"
#include "shaders/shared/types.h"

namespace static_cull {

// One prepared opaque-static draw. drawId == index into cullObjects/drawTemplates
// == the indirect command slot; firstInstance (in the template) == DrawData slot.
struct PreparedDraw {
    uint32_t blockIndex = 0;   // GeometryBuffer block to bind (3c-2 groups MDI by this)
    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t  vertexOffset = 0;
    uint32_t drawSlot = 0;     // DrawData slot == firstInstance
};

// PART4 4-前-1: one contiguous run of draws that share a GeometryBuffer block.
// build() sorts draws by blockIndex so blocks are naturally contiguous; main_pass
// then issues exactly one vkCmdDrawIndexedIndirect per BlockRange instead of
// detecting runs ad-hoc. CPU indirect calls collapse from "one per encounter run"
// (~17-18 on the current scene) down to "one per unique block" (~4).
struct BlockRange {
    uint32_t blockIndex = 0;
    uint32_t firstDraw  = 0;   // starting drawId in cullObjects/drawTemplates/draws
    uint32_t drawCount  = 0;
};

struct BuildResult {
    std::vector<myengine::shared::CullObject> cullObjects;
    std::vector<CullingPass::DrawTemplate>    drawTemplates;
    std::vector<PreparedDraw>                 draws;        // parallel to the above
    std::vector<BlockRange>                   blockRanges;  // contiguous runs after block sort
};

// cube is generated foot-based: [-0.5,0.5] x [0,1] x [-0.5,0.5].
inline AABB cubeLocalAABB() {
    AABB box{};
    box.min = glm::vec3(-0.5f, 0.0f, -0.5f);
    box.max = glm::vec3( 0.5f, 1.0f,  0.5f);
    return box;
}

// Emit one draw: push DrawData (gets the absolute slot), then the matching
// CullObject (drawId baked) + DrawTemplate (firstInstance = slot) + PreparedDraw.
inline void emit(BuildResult& result, DrawDataPool& pool, uint32_t frameIndex,
                 const glm::mat4& model, float alpha, uint32_t materialId,
                 uint32_t blockIndex, uint32_t indexCount, uint32_t firstIndex,
                 int32_t vertexOffset, const myengine::shared::CullObject& cull) {
    myengine::shared::DrawData drawData{};
    drawData.model = model;
    drawData.alpha = alpha;
    drawData.materialId = materialId;
    const uint32_t slot = pool.pushOne(frameIndex, drawData);
    if (slot == UINT32_MAX) return;  // pool full: stop emitting this frame

    const uint32_t drawId = static_cast<uint32_t>(result.draws.size());
    myengine::shared::CullObject cullObject = cull;
    cullObject.extentDrawId.w = static_cast<float>(drawId);  // cull.comp writes cmds[drawId]
    result.cullObjects.push_back(cullObject);

    CullingPass::DrawTemplate drawTemplate{};
    drawTemplate.indexCount = indexCount;
    drawTemplate.firstIndex = firstIndex;
    drawTemplate.vertexOffset = vertexOffset;
    drawTemplate.firstInstance = slot;
    result.drawTemplates.push_back(drawTemplate);

    result.draws.push_back(PreparedDraw{blockIndex, indexCount, firstIndex, vertexOffset, slot});
}

// Build for main's opaque static lists. Order: cube mesh -> static models
// (per submesh) -> terrain, in encounter order. drawId is contiguous from 0.
inline BuildResult build(DrawDataPool& pool, uint32_t frameIndex, const Mesh* cubeMesh,
                         const std::vector<MeshDrawItem>& meshList,
                         const std::vector<StaticModelDrawItem>& modelList,
                         const std::vector<TerrainDrawItem>& terrainList) {
    BuildResult result;

    // --- cube mesh draws ---
    if (cubeMesh) {
        const AABB cubeLocal = cubeLocalAABB();
        for (const MeshDrawItem& item : meshList) {
            const BoundingSphere boundingSphere = worldBoundingSphere(item.model, cubeLocal);
            myengine::shared::CullObject cullObject{};
            cullObject.centerRadius = glm::vec4(boundingSphere.center, boundingSphere.radius);
            const uint32_t materialId = item.material ? item.material->materialId() : 0u;
            emit(result, pool, frameIndex, item.model, item.alpha, materialId,
                 cubeMesh->blockIndex(), cubeMesh->indexCount(),
                 cubeMesh->firstIndex(), cubeMesh->vertexOffset(), cullObject);
        }
    }

    // --- static models (per submesh) ---
    for (const StaticModelDrawItem& item : modelList) {
        if (!item.sourceModel) continue;
        const BoundingSphere boundingSphere =
            worldBoundingSphere(item.model, item.sourceModel->localAABB());
        const std::vector<Material>& materials = item.sourceModel->materials();
        for (const SubMesh& subMesh : item.sourceModel->subMeshes()) {
            if (subMesh.indexCount == 0) continue;
            myengine::shared::CullObject cullObject{};
            cullObject.centerRadius =
                glm::vec4(boundingSphere.center, boundingSphere.radius);  // model sphere per submesh
            const uint32_t materialId =
                (subMesh.materialIndex < materials.size())
                    ? materials[subMesh.materialIndex].materialId() : 0u;
            emit(result, pool, frameIndex, item.model, item.alpha, materialId,
                 subMesh.blockIndex, subMesh.indexCount, subMesh.firstIndex,
                 subMesh.vertexOffset, cullObject);
        }
    }

    // --- terrain ---
    // PART3c scope (per START_HERE roadmap): terrain is NOT a prop. It belongs to
    // a SEPARATE GeometryBuffer bucket with its own cull + splat material path,
    // built in the streaming Phase. It is intentionally NOT emitted here; main
    // draws terrain via the legacy CPU loop (static_draw::drawTerrainList).
    (void)terrainList;

    // --- PART4 4-前-1: block sort + BlockRange emission -----------------------
    // Reorder cullObjects/drawTemplates/draws so all entries that bind the same
    // GeometryBuffer block sit next to each other. drawId is the position in
    // these arrays, so after the permutation we must re-bake
    // cullObjects[i].extentDrawId.w = i (cull.comp writes cmds[drawId]).
    // DrawData slots (firstInstance) are NOT permuted - they keep pointing at the
    // pool entries pushed in emit order, which preserves vertex-shader data.
    // stable_sort keeps the encounter order inside the same block (predictable).
    const size_t drawN = result.draws.size();
    if (drawN > 1) {
        std::vector<uint32_t> perm(drawN);
        std::iota(perm.begin(), perm.end(), 0u);
        std::stable_sort(perm.begin(), perm.end(), [&](uint32_t a, uint32_t b) {
            return result.draws[a].blockIndex < result.draws[b].blockIndex;
        });

        std::vector<myengine::shared::CullObject>  sortedCull;
        std::vector<CullingPass::DrawTemplate>     sortedTemplates;
        std::vector<PreparedDraw>                  sortedDraws;
        sortedCull     .reserve(drawN);
        sortedTemplates.reserve(drawN);
        sortedDraws    .reserve(drawN);
        for (uint32_t newId = 0; newId < drawN; ++newId) {
            const uint32_t oldId = perm[newId];
            myengine::shared::CullObject cullObject = result.cullObjects[oldId];
            cullObject.extentDrawId.w = static_cast<float>(newId);  // re-bake drawId
            sortedCull     .push_back(cullObject);
            sortedTemplates.push_back(result.drawTemplates[oldId]);
            sortedDraws    .push_back(result.draws[oldId]);
        }
        result.cullObjects   = std::move(sortedCull);
        result.drawTemplates = std::move(sortedTemplates);
        result.draws         = std::move(sortedDraws);
    }

    // Emit one BlockRange per contiguous run of identical blockIndex.
    for (uint32_t runStart = 0; runStart < drawN;) {
        const uint32_t block = result.draws[runStart].blockIndex;
        uint32_t runEnd = runStart + 1;
        while (runEnd < drawN && result.draws[runEnd].blockIndex == block) ++runEnd;
        result.blockRanges.push_back(BlockRange{block, runStart, runEnd - runStart});
        runStart = runEnd;
    }

    return result;
}

}  // namespace static_cull
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

#include <cstdint>
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

struct BuildResult {
    std::vector<myengine::shared::CullObject> cullObjects;
    std::vector<CullingPass::DrawTemplate> drawTemplates;
    std::vector<PreparedDraw> draws;   // parallel to the above, for CPU/MDI draw
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
    for (const TerrainDrawItem& item : terrainList) {
        if (!item.terrain) continue;
        myengine::shared::CullObject cullObject{};
        cullObject.centerRadius =
            glm::vec4(item.terrain->worldCenter(), item.terrain->boundingRadius());
        const uint32_t materialId = item.material ? item.material->materialId() : 0u;
        emit(result, pool, frameIndex, item.model, item.alpha, materialId,
             item.terrain->blockIndex(), item.terrain->indexCount(),
             item.terrain->firstIndex(), item.terrain->vertexOffset(), cullObject);
    }

    return result;
}

}  // namespace static_cull
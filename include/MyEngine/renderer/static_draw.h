// include/MyEngine/renderer/static_draw.h
#pragma once
// =============================================================================
// static_draw.h - Phase 2B PART3b (2b-2): shared static-draw helpers, SSBO path
// =============================================================================
// One place that records a static draw. Per-draw data (model/materialId/alpha)
// is pushed into the per-frame DrawDataPool SSBO; the returned slot is passed as
// vkCmdDrawIndexed's firstInstance, so the vertex shader reads
// DrawData[gl_InstanceIndex]. No per-draw push constant update -> indirect-ready
// (PART3c swaps the CPU loop for vkCmdDrawIndexedIndirect, same firstInstance).
//
// The caller pushes the DrawData SSBO ADDRESS once per block (StaticDrawPush-
// Constants) before calling these. materialId source differs per caller:
// main resolves the real id, reflection uses 0 (useRealMaterial).
// Header-only (inline): no new translation unit.
// =============================================================================
#include <vulkan/vulkan.h>

#include <vector>

#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/material.h"
#include "renderer/terrain_mesh.h"
#include "renderer/draw_data_pool.h"
#include "scene/scene_data.h"
#include "shaders/shared/types.h"

namespace static_draw {

using DrawData = myengine::shared::DrawData;

// Cube mesh draws (one shared Mesh, per-item model).
inline void drawMeshList(VkCommandBuffer cmd, DrawDataPool& pool, uint32_t frameIndex,
                         const Mesh* mesh, const std::vector<MeshDrawItem>& list,
                         bool useRealMaterial) {
    if (!mesh || list.empty()) return;
    for (const MeshDrawItem& item : list) {
        DrawData d{};
        d.model = item.model;
        d.alpha = item.alpha;
        d.materialId = (useRealMaterial && item.material) ? item.material->materialId() : 0u;
        const uint32_t slot = pool.pushOne(frameIndex, d);
        if (slot == UINT32_MAX) return;  // pool full: skip rest this frame
        mesh->bindAndDraw(cmd, 1, slot);
    }
}

// Static model draws (sub-mesh granular; per-submesh material id when real).
inline void drawStaticModelList(VkCommandBuffer cmd, DrawDataPool& pool, uint32_t frameIndex,
                                const std::vector<StaticModelDrawItem>& list,
                                bool useRealMaterial) {
    if (list.empty()) return;
    const Model* curModel = nullptr;
    const std::vector<Material>* curMaterials = nullptr;
    for (const StaticModelDrawItem& item : list) {
        if (!item.sourceModel) continue;
        if (item.sourceModel != curModel) {
            curModel = item.sourceModel;
            curMaterials = &curModel->materials();
        }
        for (const SubMesh& sm : curModel->subMeshes()) {
            if (sm.indexCount == 0) continue;
            DrawData d{};
            d.model = item.model;
            d.alpha = item.alpha;
            d.materialId = (useRealMaterial && curMaterials && sm.materialIndex < curMaterials->size())
                ? (*curMaterials)[sm.materialIndex].materialId()
                : 0u;
            const uint32_t slot = pool.pushOne(frameIndex, d);
            if (slot == UINT32_MAX) return;
            sm.bindAndDraw(cmd, 1, slot);
        }
    }
}

// Terrain draws (own buffer; firstIndex/vertexOffset = 0, slot via firstInstance).
inline void drawTerrainList(VkCommandBuffer cmd, DrawDataPool& pool, uint32_t frameIndex,
                            const std::vector<TerrainDrawItem>& list, bool useRealMaterial) {
    if (list.empty()) return;
    for (const TerrainDrawItem& item : list) {
        if (!item.terrain) continue;
        DrawData d{};
        d.model = item.model;
        d.alpha = item.alpha;
        d.materialId = (useRealMaterial && item.material) ? item.material->materialId() : 0u;
        const uint32_t slot = pool.pushOne(frameIndex, d);
        if (slot == UINT32_MAX) return;
        item.terrain->bind(cmd);
        // PART3c: terrain may live in the shared megabuffer now -> draw its range.
        vkCmdDrawIndexed(cmd, item.terrain->indexCount(), 1,
                         item.terrain->firstIndex(), item.terrain->vertexOffset(), slot);
    }
}

}  // namespace static_draw
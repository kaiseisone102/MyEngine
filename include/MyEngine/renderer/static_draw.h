// include/MyEngine/renderer/static_draw.h
#pragma once
// =============================================================================
// static_draw.h - Phase 2B PART3b: shared static-draw helpers (main + reflection)
// =============================================================================
// main_pass and reflection_pass had byte-identical copies of drawMeshList /
// drawStaticModelList / drawTerrainList. They are consolidated here so there is
// ONE place that knows how a static draw is recorded. Header-only (inline): no
// new translation unit, no CMakeLists entry.
//
// 2b-1 SCOPE: behaviour-preserving. Still updates StaticPushConstants per draw
// exactly as before. 2b-2 will switch the bodies to the per-draw SSBO path
// (DrawDataPool + firstInstance) without touching the call sites again.
//
// materialId differs per caller: main resolves the real material id; reflection
// uses 0. The caller passes useRealMaterial to select.
// =============================================================================
#include <vulkan/vulkan.h>

#include <vector>

#include "renderer/mesh.h"
#include "renderer/model.h"
#include "renderer/material.h"
#include "renderer/terrain_mesh.h"
#include "scene/scene_data.h"
#include "shaders/shared/types.h"

namespace static_draw {

using StaticPC = myengine::shared::StaticPushConstants;

inline void pushPC(VkCommandBuffer cmd, VkPipelineLayout layout, const StaticPC& pc) {
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(StaticPC), &pc);
}

// Cube mesh draws (one shared Mesh, per-item model).
inline void drawMeshList(VkCommandBuffer cmd, VkPipelineLayout layout, const Mesh* mesh,
                         const std::vector<MeshDrawItem>& list, bool useRealMaterial) {
    if (!mesh || list.empty()) return;
    for (const MeshDrawItem& item : list) {
        StaticPC pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        pc.materialId = (useRealMaterial && item.material) ? item.material->materialId() : 0u;
        pushPC(cmd, layout, pc);
        mesh->bindAndDraw(cmd);
    }
}

// Static model draws (sub-mesh granular; per-submesh material id when real).
inline void drawStaticModelList(VkCommandBuffer cmd, VkPipelineLayout layout,
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
        StaticPC pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        for (const SubMesh& sm : curModel->subMeshes()) {
            if (sm.indexCount == 0) continue;
            pc.materialId = (useRealMaterial && curMaterials && sm.materialIndex < curMaterials->size())
                ? (*curMaterials)[sm.materialIndex].materialId()
                : 0u;
            pushPC(cmd, layout, pc);
            sm.bindAndDraw(cmd);
        }
    }
}

// Terrain draws (own buffer; firstIndex/vertexOffset = 0).
inline void drawTerrainList(VkCommandBuffer cmd, VkPipelineLayout layout,
                            const std::vector<TerrainDrawItem>& list, bool useRealMaterial) {
    if (list.empty()) return;
    for (const TerrainDrawItem& item : list) {
        if (!item.terrain) continue;
        StaticPC pc{};
        pc.model = item.model;
        pc.alpha = item.alpha;
        pc.materialId = (useRealMaterial && item.material) ? item.material->materialId() : 0u;
        pushPC(cmd, layout, pc);
        item.terrain->bind(cmd);
        vkCmdDrawIndexed(cmd, item.terrain->indexCount(), 1, 0, 0, 0);
    }
}

}  // namespace static_draw
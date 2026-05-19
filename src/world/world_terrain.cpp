// =============================================================================
// world_terrain.cpp — 複数 TerrainMesh の集約クラス
// =============================================================================
#include "world/world_terrain.h"

#include <limits>

float WorldTerrain::sampleHeight(float worldX, float worldZ) const {
    float bestY = std::numeric_limits<float>::lowest();
    for (const auto& m : meshes_) {
        if (!m) continue;
        const float h = m->sampleHeight(worldX, worldZ);
        if (h != std::numeric_limits<float>::lowest() && h > bestY) {
            bestY = h;
        }
    }
    return bestY;
}

glm::vec3 WorldTerrain::sampleNormal(float worldX, float worldZ) const {
    const TerrainMesh* best = nullptr;
    float bestY = std::numeric_limits<float>::lowest();
    for (const auto& m : meshes_) {
        if (!m) continue;
        const float h = m->sampleHeight(worldX, worldZ);
        if (h != std::numeric_limits<float>::lowest() && h > bestY) {
            bestY = h;
            best = m.get();
        }
    }
    if (best) return best->sampleNormal(worldX, worldZ);
    return glm::vec3{0.f, 1.f, 0.f};
}

bool WorldTerrain::isInsideAny(float worldX, float worldZ) const {
    for (const auto& m : meshes_) {
        if (m && m->isInsideXZ(worldX, worldZ)) return true;
    }
    return false;
}

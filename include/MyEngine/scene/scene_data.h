#pragma once
// =============================================================================
// scene_data.h ÃÂ¢ÃÂÃÂ ÃÂ¦ÃÂÃÂÃÂ§ÃÂÃÂ»ÃÂ£ÃÂÃÂ­ÃÂ£ÃÂÃÂ¥ÃÂ£ÃÂÃÂ¼ (opaque + transparent ÃÂ¥ÃÂÃÂ¥) + Water list
// =============================================================================

#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/water.h"  // WaterDrawParams

class Material;
class Model;
class TerrainMesh;
class WaterMesh;

struct MeshDrawItem {
    glm::mat4 model{1.f};
    const Material* material = nullptr;
    float alpha = 1.f;
};

struct StaticModelDrawItem {
    glm::mat4 model{1.f};
    const Model* sourceModel = nullptr;
    float alpha = 1.f;
};

struct SkinnedDrawItem {
    glm::mat4 model{1.f};
    const Model* sourceModel = nullptr;
    int skinOffset = 0;
    float alpha = 1.f;
};

struct TerrainDrawItem {
    glm::mat4 model{1.f};
    const TerrainMesh* terrain = nullptr;
    const Material* material = nullptr;
    float alpha = 1.f;
};

struct WaterDrawItem {
    glm::vec3 center{0.f};
    glm::vec2 sizeXZ{16.f, 16.f};
    const WaterMesh* mesh = nullptr;
    WaterDrawParams drawParams;
};

class SceneData {
   public:
    // ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ opaque (ÃÂ¤ÃÂ¸ÃÂÃÂ©ÃÂÃÂÃÂ¦ÃÂÃÂ) ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ
    const std::vector<MeshDrawItem>& meshDrawListOpaque() const { return meshOpaque_; }
    std::vector<MeshDrawItem>& meshDrawListOpaque() { return meshOpaque_; }
    const std::vector<StaticModelDrawItem>& staticModelDrawListOpaque() const { return staticOpaque_; }
    std::vector<StaticModelDrawItem>& staticModelDrawListOpaque() { return staticOpaque_; }
    const std::vector<SkinnedDrawItem>& modelDrawListOpaque() const { return modelOpaque_; }
    std::vector<SkinnedDrawItem>& modelDrawListOpaque() { return modelOpaque_; }
    const std::vector<TerrainDrawItem>& terrainDrawListOpaque() const { return terrainOpaque_; }
    std::vector<TerrainDrawItem>& terrainDrawListOpaque() { return terrainOpaque_; }

    // ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ transparent (ÃÂ¥ÃÂÃÂÃÂ©ÃÂÃÂÃÂ¦ÃÂÃÂ) ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ
    const std::vector<MeshDrawItem>& meshDrawListTransparentConst() const { return meshTransparent_; }
    std::vector<MeshDrawItem>& meshDrawListTransparent() { return meshTransparent_; }
    const std::vector<StaticModelDrawItem>& staticModelDrawListTransparentConst() const {
        return staticTransparent_;
    }
    std::vector<StaticModelDrawItem>& staticModelDrawListTransparent() { return staticTransparent_; }
    const std::vector<SkinnedDrawItem>& modelDrawListTransparentConst() const {
        return modelTransparent_;
    }
    std::vector<SkinnedDrawItem>& modelDrawListTransparent() { return modelTransparent_; }
    const std::vector<TerrainDrawItem>& terrainDrawListTransparentConst() const {
        return terrainTransparent_;
    }
    std::vector<TerrainDrawItem>& terrainDrawListTransparent() { return terrainTransparent_; }

    // ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ Water ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ
    const std::vector<WaterDrawItem>& waterDrawList() const { return waters_; }
    std::vector<WaterDrawItem>& waterDrawList() { return waters_; }

    // ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ culling distance ÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂÃÂ¢ÃÂÃÂ
    float cullingDistance() const { return cullingDistance_; }
    void setCullingDistance(float d) { cullingDistance_ = d; }

    void clear() {
        meshOpaque_.clear();
        staticOpaque_.clear();
        modelOpaque_.clear();
        terrainOpaque_.clear();
        meshTransparent_.clear();
        staticTransparent_.clear();
        modelTransparent_.clear();
        terrainTransparent_.clear();
        waters_.clear();
    }

   private:
    std::vector<MeshDrawItem> meshOpaque_;
    std::vector<StaticModelDrawItem> staticOpaque_;
    std::vector<SkinnedDrawItem> modelOpaque_;
    std::vector<TerrainDrawItem> terrainOpaque_;

    std::vector<MeshDrawItem> meshTransparent_;
    std::vector<StaticModelDrawItem> staticTransparent_;
    std::vector<SkinnedDrawItem> modelTransparent_;
    std::vector<TerrainDrawItem> terrainTransparent_;

    std::vector<WaterDrawItem> waters_;

    float cullingDistance_ = 100.f;
};

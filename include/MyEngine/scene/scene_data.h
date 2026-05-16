// include/MyEngine/scene/scene_data.h
#pragma once
// =============================================================================
// scene_data.h — Phase 5-F (Lighting/Shadow 強化)
// =============================================================================
// Phase 5-B 追加 (既):
//   StaticModelDrawItem  : 装備品など静的 Model
//   addStaticModelObject(): staticModelDrawList_ に追加
//
// Phase 5-F 追加:
//   setPlayerCenter()    : ライト視点を Player 中心に追従させるための位置
//                          影が常に Player 周辺に生成される (Player が移動しても消えない)
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <algorithm>
#include <glm/glm.hpp>
#include <vector>

#include "renderer/frame_uniforms.h"

class Model;

struct SkinnedDrawItem {
    glm::mat4 model{1.f};
    int32_t skinOffset = 0;
    const Model* sourceModel = nullptr;
};

struct StaticModelDrawItem {
    glm::mat4 model{1.f};
    const Model* sourceModel = nullptr;
};

class SceneData {
   public:
    void setViewProjection(const glm::mat4& view, const glm::mat4& proj) {
        view_ = view;
        proj_ = proj;
    }
    const glm::mat4& view() const { return view_; }
    const glm::mat4& projection() const { return proj_; }

    void clearObjects() {
        meshDrawList_.clear();
        modelDrawList_.clear();
        staticModelDrawList_.clear();
    }

    void addMeshObject(const glm::mat4& modelMatrix) { meshDrawList_.push_back(modelMatrix); }

    void addModelObject(const glm::mat4& modelMatrix, int32_t skinOffset,
                        const Model* sourceModel) {
        modelDrawList_.push_back({modelMatrix, skinOffset, sourceModel});
    }

    void addStaticModelObject(const glm::mat4& modelMatrix, const Model* sourceModel) {
        staticModelDrawList_.push_back({modelMatrix, sourceModel});
    }

    void addObject(const glm::mat4& modelMatrix) { addMeshObject(modelMatrix); }

    void sortModelDrawListBySourceModel() {
        std::sort(modelDrawList_.begin(), modelDrawList_.end(),
                  [](const SkinnedDrawItem& a, const SkinnedDrawItem& b) {
                      return a.sourceModel < b.sourceModel;
                  });
    }

    void sortStaticModelDrawListBySourceModel() {
        std::sort(staticModelDrawList_.begin(), staticModelDrawList_.end(),
                  [](const StaticModelDrawItem& a, const StaticModelDrawItem& b) {
                      return a.sourceModel < b.sourceModel;
                  });
    }

    const std::vector<glm::mat4>& meshDrawList() const { return meshDrawList_; }
    const std::vector<SkinnedDrawItem>& modelDrawList() const { return modelDrawList_; }
    const std::vector<StaticModelDrawItem>& staticModelDrawList() const {
        return staticModelDrawList_;
    }

    void setLightingParams(const glm::vec3& lightPos, const glm::vec3& lightColor,
                           const glm::vec3& viewPos, float ambient = 0.15f, float specular = 0.5f) {
        lightPos_ = lightPos;
        lightColor_ = lightColor;
        viewPos_ = viewPos;
        ambient_ = ambient;
        specular_ = specular;
    }
    void setShadowParams(float strength, float bias) {
        shadowStrength_ = strength;
        shadowBias_ = bias;
    }

    // Phase 5-F: ライト視点の中心位置 (Player 位置を指定)
    // toLightingData() でこの位置を中心にライト正射影を構築する。
    void setPlayerCenter(const glm::vec3& p) { playerCenter_ = p; }

    FrameUniforms::LightingData toLightingData() const;

   private:
    glm::mat4 view_{1.f};
    glm::mat4 proj_{1.f};

    std::vector<glm::mat4> meshDrawList_;
    std::vector<SkinnedDrawItem> modelDrawList_;
    std::vector<StaticModelDrawItem> staticModelDrawList_;

    glm::vec3 lightPos_{10.f, 20.f, 10.f};
    glm::vec3 lightColor_{1.f, 1.f, 1.f};
    glm::vec3 viewPos_{0.f, 0.f, 5.f};
    float ambient_ = 0.15f;
    float specular_ = 0.5f;
    float shadowStrength_ = 0.8f;   // Phase 5-F: 0.6 -> 0.8 (強化)
    float shadowBias_     = 0.0015f; // Phase 5-F: 0.003 -> 0.0015 (細かく)

    // Phase 5-F: ライト視点の中心 (Player 位置)
    // setPlayerCenter() で設定。 デフォルトは原点。
    glm::vec3 playerCenter_{0.f, 0.f, 0.f};
};

#pragma once
// =============================================================================
// scene_renderer.h — flecs world → SceneData 構築
// =============================================================================
// 各 frame:
//   1. SceneData::clear()
//   2. 各 entity の CTransform + CStaticModelRef 等を見て DrawItem を作る
//   3. cameraPos からの距離で cull (drawDistance 超えは追加しない)
//   4. alpha < 1 や fading 中なら transparent list に振り分け
//   5. CWater を見て waterList に追加
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct WorldData;
class SceneData;

class SceneRenderer {
   public:
    void buildSceneData(const WorldData& wd, const glm::vec3& cameraPos, SceneData& out,
                         float cullingDistance);
};

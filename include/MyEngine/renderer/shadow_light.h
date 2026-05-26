#pragma once
// =============================================================================
// shadow_light.h — directional シャドウマップ用の正射影行列 (重複解消)
//   title_layer / camera_system が同一の glm::ortho(-15,15,-15,15,0.1,50) +
//   Vulkan Y 反転を別々に書いていたのを 1 箇所に集約したもの。
//   ※ これは純粋な重複解消であり挙動は不変。ライト VP の安定化 (texel
//     snapping) やカスケード化 (CSM) は Phase 2E で別途扱う (ここでは行わない)。
//   view 行列 (lookAt) は呼び出し側ごとに異なる (title=固定 target /
//   camera=playerPos) ため、ここでは射影のみを共通化する。
// =============================================================================
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace shadow_light {

// directional ライトの正射影 (Vulkan Y 反転込み)。
// 現状はシーン固定サイズ (-15..15 xy, 0.1..50 z) のハンドクラフト値。
// タイトな fit / 安定化は Phase 2E で置き換える。
inline glm::mat4 directionalLightProj() {
    glm::mat4 proj = glm::ortho(-15.f, 15.f, -15.f, 15.f, 0.1f, 50.f);
    proj[1][1] *= -1.f;  // Vulkan Y flip
    return proj;
}

}  // namespace shadow_light
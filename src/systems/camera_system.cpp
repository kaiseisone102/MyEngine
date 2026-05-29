// =============================================================================
// camera_system.cpp — 入力抽象化対応
// + Phase 1C: applyViewProjectionAndLighting を SceneData API 廃止に対応
//   旧: scene.setViewProjection / setPlayerCenter / setLightingParams
//   新: LightingUBO を構築して renderer.setLighting() で渡す
// =============================================================================
#include "systems/camera_system.h"

#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/action_state.h"
#include "renderer/frame_uniforms.h"
#include "renderer/shadow_light.h"
#include "renderer/vulkan_renderer.h"
#include "world/engine_origin.h"  // E: camera-relative wire-up

void CameraSystem::toggleMode(Camera& camera) const { camera.toggleMode(); }

void CameraSystem::applyMouseLook(Camera& camera, bool mouseCapture, float xrel, float yrel) const {
    if (!mouseCapture) return;
    camera.processMouseMovement(xrel * sensitivity_, yrel * sensitivity_);
}

void CameraSystem::updateFpsMove(Camera& camera, bool inputLocked, const ActionState& input,
                                 float dt) const {
    if (inputLocked) return;
    // ActionState → bool 復元 (Camera::processFpsKeyboard の API を維持)
    const bool fwd = input.moveZ > 0.5f;
    const bool back = input.moveZ < -0.5f;
    const bool left = input.moveX < -0.5f;
    const bool right = input.moveX > 0.5f;
    const bool up = input.moveUp;
    const bool down = input.moveDown;
    camera.processFpsKeyboard(fwd, back, left, right, up, down, dt);
}

void CameraSystem::applyViewProjectionAndLighting(VulkanRenderer& renderer, const Camera& camera,
                                                    const glm::vec3& playerPos, int winW,
                                                    int winH) const {
    const float aspect = (winH > 0) ? static_cast<float>(winW) / static_cast<float>(winH) : 1.f;

    FrameUniforms::LightingUBO l{};

    // E: camera-relative coordinate convention. Every world-space matrix
    // and position uploaded to the GPU has EngineOrigin::current()
    // subtracted from its translation column, so when the origin shifts
    // (floating-origin rebase) the cull / shader pipeline keeps reading
    // small-magnitude coordinates with full float precision. Today
    // current() returns (0,0,0); the subtraction is a numeric no-op but
    // the path is wired so the upgrade flips on without revisiting every
    // matrix construction site. See world/engine_origin.h.
    const glm::vec3 origin = myengine::world::EngineOrigin::current();
    const glm::vec3 cameraWorld = camera.getCameraWorldPosition(playerPos);

    // ─── 通常 view + proj ─────────────────────────────────────
    // view_rel = view_world * translate(origin). The composition
    // view_rel * model_rel = view_world * translate(origin) *
    //   translate(-origin) * model_world = view_world * model_world
    // so the math is identical when origin == 0.
    l.view = camera.getViewMatrix(playerPos) *
             glm::translate(glm::mat4(1.f), origin);
    l.proj = camera.getProjectionMatrix(aspect);

    // ─── shadow camera (旧 scene_data.cpp::toLightingData 由来) ─
    // ライト位置と target は旧 setLightingParams 引数を継承。
    // ortho は -15〜15 (xy)、 0.1〜50 (z)。
    // E: lightView is built in the same camera-relative space so it
    // composes cleanly with model_rel in static_cull_build::emit().
    const glm::vec3 lightPos{8.f, 15.f, 8.f};
    const glm::vec3 target = playerPos;
    const glm::mat4 lightView =
        glm::lookAt(lightPos - origin, target - origin, glm::vec3(0.f, 1.f, 0.f));
    const glm::mat4 lightProj = shadow_light::directionalLightProj();
    l.lightVP = lightProj * lightView;

    // ─── shader が参照する追加項目 (Phase 1C 整合) ─────────────
    // shader 側は lightDir.xyz / lightColor.rgb / ambient.rgb / viewPos.xyz / shadowParams.x のみ使用。
    // E: lightDir is a direction (translation-invariant) so origin
    // cancels: (target - origin) - (lightPos - origin) = target - lightPos.
    l.lightDir = glm::vec4(glm::normalize(target - lightPos), 0.f);
    l.lightColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
    l.ambient = glm::vec4(0.15f, 0.15f, 0.15f, 0.f);
    // E: viewPos is engine-relative; fragment shaders subtract a
    // model_rel-transformed fragPos which is in the same space, so
    // V = normalize(viewPos - fragPos) is correct after the wire-up.
    l.viewPos = glm::vec4(cameraWorld - origin, 0.f);
    l.shadowParams = glm::vec4(0.6f, 0.f, 0.f, 0.f);  // x = strength

    renderer.setLighting(l);
}

void CameraSystem::setSensitivity(float s) { sensitivity_ = std::clamp(s, 0.05f, 5.0f); }

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
#include "renderer/vulkan_renderer.h"

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

    // ─── 通常 view + proj ─────────────────────────────────────
    l.view = camera.getViewMatrix(playerPos);
    l.proj = camera.getProjectionMatrix(aspect);

    // ─── shadow camera (旧 scene_data.cpp::toLightingData 由来) ─
    // ライト位置と target は旧 setLightingParams 引数を継承。
    // ortho は -15〜15 (xy)、 0.1〜50 (z)。
    // lightProj[1][1] *= -1.f は Vulkan Y 反転 (main camera との整合)。
    const glm::vec3 lightPos{8.f, 15.f, 8.f};
    const glm::vec3 target = playerPos;
    const glm::mat4 lightView = glm::lookAt(lightPos, target, glm::vec3(0.f, 1.f, 0.f));
    glm::mat4 lightProj = glm::ortho(-15.f, 15.f, -15.f, 15.f, 0.1f, 50.f);
    lightProj[1][1] *= -1.f;
    l.lightVP = lightProj * lightView;

    // ─── shader が参照する追加項目 (Phase 1C 整合) ─────────────
    // shader 側は lightDir.xyz / lightColor.rgb / ambient.rgb / viewPos.xyz / shadowParams.x のみ使用。
    l.lightDir = glm::vec4(glm::normalize(target - lightPos), 0.f);
    l.lightColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
    l.ambient = glm::vec4(0.15f, 0.15f, 0.15f, 0.f);
    l.viewPos = glm::vec4(camera.getCameraWorldPosition(playerPos), 0.f);
    l.shadowParams = glm::vec4(0.6f, 0.f, 0.f, 0.f);  // x = strength

    renderer.setLighting(l);
}

void CameraSystem::setSensitivity(float s) { sensitivity_ = std::clamp(s, 0.05f, 5.0f); }

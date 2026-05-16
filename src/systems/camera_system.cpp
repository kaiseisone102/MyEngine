// C:\MyEngine\src\systems\camera_system.cpp

#include "systems/camera_system.h"

void CameraSystem::toggleMode(Camera& camera) const { camera.toggleMode(); }

void CameraSystem::applyMouseLook(Camera& camera, bool mouseCapture, float xrel, float yrel) const {
    if (!mouseCapture) return;
    camera.processMouseMovement(xrel, yrel);
}

void CameraSystem::updateFpsMove(Camera& camera, bool inputLocked, const bool* keys,
                                 float dt) const {
    if (inputLocked) return;
    camera.processFpsKeyboard(keys[SDL_SCANCODE_W], keys[SDL_SCANCODE_S], keys[SDL_SCANCODE_A],
                              keys[SDL_SCANCODE_D], keys[SDL_SCANCODE_E], keys[SDL_SCANCODE_Q], dt);
}

void CameraSystem::applyViewProjectionAndLighting(VulkanRenderer& renderer, const Camera& camera,
                                                  const glm::vec3& playerPos, int winW,
                                                  int winH) const {
    const float aspect = (winH > 0) ? static_cast<float>(winW) / static_cast<float>(winH) : 1.f;
    // リファクタ Step 10 以降、シーン状態は SceneData に移動済み。
    // VulkanRenderer::scene() 経由でアクセスする。
    auto& scene = renderer.scene();
    scene.setViewProjection(camera.getViewMatrix(playerPos), camera.getProjectionMatrix(aspect));
    scene.setLightingParams({8.f, 15.f, 8.f}, {1.f, 1.f, 1.f},
                            camera.getCameraWorldPosition(playerPos), 0.15f, 0.5f);
}

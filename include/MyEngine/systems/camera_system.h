#pragma once

#include <SDL3/SDL.h>

#include "core/camera.h"
#include "renderer/vulkan_renderer.h"

struct ActionState;

class CameraSystem {
   public:
    void toggleMode(Camera& camera) const;
    void applyMouseLook(Camera& camera, bool mouseCapture, float xrel, float yrel) const;

    // 入力抽象化: keys → ActionState
    void updateFpsMove(Camera& camera, bool inputLocked, const ActionState& input,
                       float dt) const;

    void applyViewProjectionAndLighting(VulkanRenderer& renderer, const Camera& camera,
                                        const glm::vec3& playerPos, int winW, int winH) const;

    void setSensitivity(float s);
    float sensitivity() const { return sensitivity_; }

   private:
    float sensitivity_ = 1.0f;
};

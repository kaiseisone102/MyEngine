#pragma once

#include <SDL3/SDL.h>

#include "camera.h"
#include "/renderer/vulkan_triangle_renderer.h"

class CameraSystem {
public:
    void toggleMode(Camera& camera) const;
    void applyMouseLook(Camera& camera, bool mouseCapture, float xrel, float yrel) const;
    void updateFpsMove(Camera& camera, bool inputLocked, const bool* keys, float dt) const;
    void applyViewProjectionAndLighting(VulkanTriangleRenderer& renderer,
                                        const Camera& camera,
                                        const glm::vec3& playerPos,
                                        int winW,
                                        int winH) const;
};

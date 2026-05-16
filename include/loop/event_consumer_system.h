#pragma once

#include <functional>

#include <flecs.h>

#include "/systems/audio_event_system.h"
#include "/systems/camera_system.h"
#include "/systems/combat_system.h"
#include "/core/event_bus.h"
#include "/systems/sound_manager.h"
#include "/renderer/vulkan_triangle_renderer.h"

class EventConsumerSystem {
public:
    void consume(EventBus& bus,
                 bool& running,
                 bool& mouseCapture,
                 Camera& camera,
                 CameraSystem& cameraSystem,
                 CombatSystem& combatSystem,
                 AudioEventSystem& audioEventSystem,
                 flecs::entity player,
                 SoundManager& sound,
                 VulkanTriangleRenderer& renderer,
                 const std::function<void(bool)>& setMouseCapture) const;
};

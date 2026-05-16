#pragma once

#include <flecs.h>

#include <functional>

#include "core/event_bus.h"
#include "renderer/vulkan_renderer.h"
#include "systems/audio_event_system.h"
#include "systems/camera_system.h"
#include "systems/combat_system.h"
#include "systems/sound_manager.h"

class EventConsumerSystem {
   public:
    void consume(EventBus& bus, bool& running, bool& mouseCapture, Camera& camera,
                 CameraSystem& cameraSystem, CombatSystem& combatSystem,
                 AudioEventSystem& audioEventSystem, flecs::entity player, SoundManager& sound,
                 VulkanRenderer& renderer, const std::function<void(bool)>& setMouseCapture) const;
};

#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>
#include <flecs.h>

#include "/systems/audio_event_system.h"
#include "/systems/item_pickup_system.h"
#include "camera.h"
#include "/systems/camera_system.h"
#include "/systems/combat_system.h"
#include "/systems/enemy_system.h"
#include "event_bus.h"
#include "/loop/event_consumer_system.h"
#include "/systems/input_system.h"
#include "/systems/movement_system.h"
#include "/systems/physics_system.h"
#include "/systems/render_debug_system.h"
#include "/systems/sound_manager.h"
#include "/systems/spawn_system.h"
#include "/core/spawn_trigger.h"
#include "/renderer/vulkan_triangle_renderer.h"

struct RuntimeState {
    SDL_Window* window = nullptr;
    bool sdlVulkan = false;
    Camera camera;
    bool mouseCapture = false;
    uint64_t lastTicks = 0;

    float fps = 0.f;
    float fpsAccum = 0.f;
    int fpsFrames = 0;
};

struct WorldData {
    VulkanTriangleRenderer vulkan;
    flecs::world world;
    flecs::entity player;
    std::vector<flecs::entity> platforms;
    std::vector<flecs::entity> enemies;        // 敵エンティティ一覧
    std::vector<SpawnTrigger>  spawnTriggers;  // スポーントリガー一覧
    std::vector<flecs::entity> shieldItems;    // マップ上の盾アイテム一覧
    std::vector<flecs::entity> armorItems;     // マップ上のアーマーアイテム（区画+1）
};

struct SystemsRegistry {
    SoundManager sound;
    EventBus eventBus;
    InputSystem inputSystem;
    EventConsumerSystem eventConsumerSystem;
    AudioEventSystem audioEventSystem;
    CombatSystem combatSystem;
    MovementSystem movementSystem;
    PhysicsSystem physicsSystem;
    CameraSystem cameraSystem;
    RenderDebugSystem renderDebugSystem;
    EnemySystem      enemySystem;        // 敵 AI 更新システム
    SpawnSystem      spawnSystem;        // スポーントリガーシステム
    ItemPickupSystem itemPickupSystem;   // アイテム拾得システム
};

struct WorldState {
    WorldData data;
    SystemsRegistry systems;
};

struct GameState {
    RuntimeState runtime;
    WorldState worldState;
};

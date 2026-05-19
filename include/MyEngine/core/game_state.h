#pragma once
// =============================================================================
// game_state.h — + WorldWater waters
// =============================================================================

#include <SDL3/SDL.h>
#include <flecs.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "camera.h"
#include "event_bus.h"
#include "game_settings.h"
#include "renderer/terrain_mesh.h"
#include "world/world_terrain.h"
#include "world/world_water.h"
#include "renderer/vulkan_renderer.h"
#include "spawn_trigger.h"
#include "systems/anim_state_system.h"
#include "systems/audio_event_system.h"
#include "systems/camera_system.h"
#include "systems/chest_system.h"
#include "systems/combat_system.h"
#include "systems/enemy_system.h"
#include "systems/gate_system.h"
#include "systems/item_physics_system.h"
#include "systems/item_pickup_system.h"
#include "systems/movement_system.h"
#include "systems/moving_platform_system.h"
#include "systems/particle_system.h"
#include "systems/physics_system.h"
#include "systems/render_debug_system.h"
#include "systems/skeletal_anim_system.h"
#include "systems/sound_manager.h"
#include "systems/spawn_system.h"
#include "systems/spin_animation_system.h"
#include "systems/spirit_system.h"

struct DebugFlags {
    bool showHitboxes   = false;
    bool overlayVisible = false;
    RenderDebugSystem::Corner overlayCorner = RenderDebugSystem::Corner::BottomLeft;
};

struct RuntimeState {
    SDL_Window* window = nullptr;
    bool sdlVulkan = false;
    Camera camera;
    bool mouseCapture = false;
    uint64_t lastTicks = 0;

    float fps = 0.f;
    float fpsAccum = 0.f;
    int fpsFrames = 0;

    std::string topLayerName;
    int layerStackDepth = 1;

    DebugFlags debug;
};

struct WorldData {
    VulkanRenderer vulkan;
    flecs::world world;
    flecs::entity player;
    std::vector<flecs::entity> platforms;
    std::vector<flecs::entity> enemies;
    std::vector<SpawnTrigger> spawnTriggers;
    std::vector<flecs::entity> shieldItems;
    std::vector<flecs::entity> armorItems;
    std::vector<flecs::entity> gripItems;
    std::vector<flecs::entity> keyItems;
    std::vector<flecs::entity> moneyItems;
    std::vector<flecs::entity> potionItems;
    std::vector<flecs::entity> spiritItems;
    std::vector<flecs::entity> decorations;
    std::vector<flecs::entity> gates;
    std::vector<flecs::entity> chests;
    std::vector<flecs::entity> graves;
    std::vector<flecs::entity> obstacles;
    WorldTerrain terrains;
    WorldWater waters;
};

struct SystemsRegistry {
    SoundManager sound;
    EventBus eventBus;
    AudioEventSystem audioEventSystem;
    CombatSystem combatSystem;
    MovementSystem movementSystem;
    PhysicsSystem physicsSystem;
    CameraSystem cameraSystem;
    RenderDebugSystem renderDebugSystem;
    EnemySystem enemySystem;
    SpawnSystem spawnSystem;
    ItemPickupSystem itemPickupSystem;
    ItemPhysicsSystem itemPhysicsSystem;
    SkeletalAnimSystem skeletalAnimSystem;
    AnimStateSystem animStateSystem;
    SpinAnimationSystem spinAnimationSystem;
    ParticleSystem particleSystem;
    MovingPlatformSystem movingPlatformSystem;
    GateSystem gateSystem;
    ChestSystem chestSystem;
    SpiritSystem spiritSystem;
};

struct WorldState {
    WorldData data;
    SystemsRegistry systems;
};

struct GameState {
    RuntimeState runtime;
    WorldState worldState;
    GameSettings settings;
};

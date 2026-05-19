// \MyEngine\src\app\engine_app.cpp
// =============================================================================
// + settings ロードを vulkan.init の前に移動 + reflection quality 反映
// + Phase 1C: render::SceneRenderer → SceneRenderer (グローバル namespace に統一)
//            setCullingDistance 廃止 (SceneRenderer は cullingDistance を保持しない)
// =============================================================================
#include "app/engine_app.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <stdexcept>
#include <string>

#include "core/components.h"
#include "core/settings_io.h"
#include "renderer/model_loader.h"
#include "systems/spawn_system.h"
namespace {
std::string getSystemVulkanLoaderPath() {
    char sysDir[MAX_PATH]{};
    if (GetSystemDirectoryA(sysDir, MAX_PATH) != 0) {
        return std::string(sysDir) + "\\vulkan-1.dll";
    }
    return "C:\\Windows\\System32\\vulkan-1.dll";
}
struct ModelDef {
    const char* name;
    const char* relPath;
};
struct AnimationDef {
    const char* name;
    const char* relPath;
};
constexpr ModelDef kModelDefs[] = {
    {"knight", "knight.glb"},
    {"skeleton", "skeleton.glb"},
    {"soldier", "soldier.glb"},
    {"shield_iron", "3d_models/shields/shield_iron.glb"},
    {"shield_silver", "3d_models/shields/shield_silver.glb"},
    {"shield_gold", "3d_models/shields/shield_gold.glb"},
    {"sword_std", "3d_models/swords/sword_std.glb"},
    {"grave_1", "3d_models/graves/grave_1.glb"},
    {"grave_spirit", "3d_models/graves/grave_spirit.glb"},
    {"rock_1", "3d_models/rocks/rock_1.glb"},
    {"tree_noLeaves_1", "3d_models/trees/tree_noLeaves_1.glb"},
    {"tree_noLeaves_2", "3d_models/trees/tree_noLeaves_2.glb"},
    {"fire_grip", "3d_models/grips/fire_grip.glb"},
    {"light_grip", "3d_models/grips/light_grip.glb"},
    {"gold_key", "3d_models/keys/gold_key.glb"},
    {"silver_key", "3d_models/keys/silver_key.glb"},
    {"armor", "3d_models/armors/armor.glb"},
    {"coin", "3d_models/moneys/coin.glb"},
    {"coin_bag", "3d_models/moneys/coin_bag.glb"},
    {"diamond", "3d_models/moneys/diamond.glb"},
    {"unlocked_chest", "3d_models/chests/unlocked_chest.glb"},
    {"locked_chest", "3d_models/chests/locked_chest.glb"},
    {"spirit", "3d_models/spirits/spirit.glb"},
    {"potion_s", "3d_models/potions/potion_s.glb"},
};
constexpr AnimationDef kAnimationDefs[] = {
    {"idle", "animations/idling/idle.glb"},
    {"walk", "animations/walking/walk.glb"},
    {"jump", "animations/jumping/jump.glb"},
    {"fall", "animations/falling/fall.glb"},
    {"land", "animations/landing/land.glb"},
    {"slash", "animations/attacking/slash/slash.glb"},
    {"smash", "animations/attacking/smash/smash.glb"},
    {"smashdown", "animations/attacking/smash_down/smash_down.glb"},
    {"run", "animations/running/run.glb"},
    {"death", "animations/death_animation/death.glb"},
    {"enemy_idle", "animations/enemy_animation/idling/enemy_idle.glb"},
    {"enemy_walk", "animations/enemy_animation/walking/enemy_walk.glb"},
    {"enemy_attack", "animations/enemy_animation/attacking/enemy_attack.glb"},
    {"enemy_hit", "animations/enemy_animation/hit_reaction/enemy_hit.glb"},
    {"enemy_death", "animations/enemy_animation/death_animation/enemy_death.glb"},
};
}  // namespace
void EngineApp::initWindow() {
    const std::string loaderPath = getSystemVulkanLoaderPath();
    SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, loaderPath.c_str());
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    if (!SDL_Vulkan_LoadLibrary(loaderPath.c_str())) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_Vulkan_LoadLibrary failed: ") + SDL_GetError());
    }
    state_.runtime.sdlVulkan = true;
    state_.runtime.window = SDL_CreateWindow("MyEngine  [Space:Jump / Tab:Cam / ESC:Mouse]", 1280,
                                             720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!state_.runtime.window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }
}
void EngineApp::initGame() {
    state_.settings = settings_io::load(settings_io::defaultSettingsPath());
    std::cout << "Vulkan initializing..." << std::endl;
    state_.worldState.data.vulkan.init(state_.runtime.window);
    std::cout << "Vulkan ready." << std::endl;
    if (state_.settings.reflectionQuality != ReflectionQuality::Half) {
        state_.worldState.data.vulkan.setReflectionQuality(state_.settings.reflectionQuality);
    }
    // Phase 1C: グローバル SceneRenderer、 引数なし、 cullingDistance も保持しない
    sceneRenderer_ = std::make_unique<SceneRenderer>();
    {
        const char* base = SDL_GetBasePath();
        const std::string assetsDir = base ? std::string(base) + "assets/" : "assets/";
        AssetRegistry& assets = state_.worldState.data.vulkan.assets();
        for (const auto& def : kModelDefs) {
            const std::string fullPath = assetsDir + def.relPath;
            if (!assets.registerModel(def.name, fullPath)) {
                std::cerr << "[EngineApp] WARNING: model load failed: " << def.name << "\n";
            }
        }
        for (const auto& def : kAnimationDefs) {
            const std::string fullPath = assetsDir + def.relPath;
            if (!assets.registerAnimation(def.name, fullPath)) {
                std::cerr << "[EngineApp] WARNING: animation load failed: " << def.name << "\n";
            }
        }
    }
    state_.runtime.lastTicks = SDL_GetTicks();
    const char* base = SDL_GetBasePath();
    std::string assetsDir = base ? std::string(base) + "assets/" : "assets/";
    state_.worldState.systems.sound.init(assetsDir);
    state_.worldState.systems.sound.playBGM();
    state_.worldState.systems.sound.setBGMVolume(state_.settings.bgmVolume);
    state_.worldState.systems.sound.setSFXVolume(state_.settings.sfxVolume);
    state_.worldState.systems.cameraSystem.setSensitivity(state_.settings.mouseSensitivity);
    // Phase 1C: setCullingDistance は廃止。 drawDistance は state_.settings 経由。
    layerFactory_ =
        std::make_unique<DefaultLayerFactory>(state_, *sceneRenderer_, kGravity, kJumpSpeed);
}
void EngineApp::cleanup() {
    state_.runtime.mouseCapture = false;
    if (state_.runtime.window) {
        SDL_SetWindowRelativeMouseMode(state_.runtime.window, false);
    }
    SDL_ShowCursor();
    state_.worldState.systems.sound.shutdown();
    layerFactory_.reset();
    sceneRenderer_.reset();
    state_.worldState.data.vulkan.shutdown();
    if (state_.runtime.window) {
        SDL_DestroyWindow(state_.runtime.window);
        state_.runtime.window = nullptr;
    }
    if (state_.runtime.sdlVulkan) {
        SDL_Vulkan_UnloadLibrary();
        state_.runtime.sdlVulkan = false;
    }
    SDL_Quit();
}
void EngineApp::run() {
    initWindow();
    struct CleanupGuard {
        EngineApp& app;
        ~CleanupGuard() { app.cleanup(); }
    } guard{*this};
    initGame();
    orchestrator_.run(state_, *layerFactory_, *sceneRenderer_, kGravity, kJumpSpeed);
}

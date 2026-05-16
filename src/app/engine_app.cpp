// \MyEngine\src\app\engine_app.cpp
// =============================================================================
// engine_app.cpp — + light_grip モデル登録
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
    {"shield_iron", "shield_iron.glb"},
    {"shield_silver", "shield_silver.glb"},
    {"shield_gold", "shield_gold.glb"},
    {"sword_std", "sword_std.glb"},
    {"grave_1", "3d_models/grave/grave_1.glb"},
    {"grave_spirit", "3d_models/grave/grave_spirit.glb"},
    {"rock_1", "3d_models/rock/rock_1.glb"},
    {"tree_noLeaves_1", "3d_models/tree/tree_noLeaves_1.glb"},
    {"tree_noLeaves_2", "3d_models/tree/tree_noLeaves_2.glb"},
    // Grip アイテム
    {"fire_grip", "3d_models/grips/fire_grip.glb"},
    {"light_grip", "3d_models/grips/light_grip.glb"},
    // key アイテム
    {"gold_key", "3d_models/keys/gold_key.glb"},
    {"silver_key", "3d_models/keys/silver_key.glb"},
    // armor アイテム
    {"armor", "3d_models/armors/armor.glb"},
    // money アイテム
    {"coin", "3d_models/moneys/coin.glb"},
    {"coin_bag", "3d_models/moneys/coin_bag.glb"},
    {"diamond", "3d_models/moneys/diamond.glb"},
    // chest
    {"unlocked_chest", "3d_models/chest/unlocked_chest.glb"},
    {"locked_chest", "3d_models/chest/locked_chest.glb"},
    // spirit (墓から飛び出すアイテム)
    {"spirit", "3d_models/spirits/spirit.glb"},
    // potion アイテム
    {"potion_s", "3d_models/potions/potion_s.glb"},

};

constexpr AnimationDef kAnimationDefs[] = {
    {"idle", "idle.glb"},
    {"walk", "walk.glb"},
    {"jump", "jump.glb"},
    {"fall", "fall.glb"},
    {"land", "land.glb"},
    {"slash", "slash.glb"},
    {"smash", "smash.glb"},
    {"smashdown", "smash_down.glb"},
    {"run", "run.glb"},
    {"death", "death.glb"},
    {"enemy_idle", "enemy_idle.glb"},
    {"enemy_walk", "enemy_walk.glb"},
    {"enemy_attack", "enemy_attack.glb"},
    {"enemy_hit", "enemy_hit.glb"},
    {"enemy_death", "enemy_death.glb"},
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
    std::cout << "Vulkan initializing..." << std::endl;
    state_.worldState.data.vulkan.init(state_.runtime.window);
    std::cout << "Vulkan ready." << std::endl;

    sceneRenderer_ = std::make_unique<render::SceneRenderer>(state_.worldState.data.vulkan);

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

    state_.settings = settings_io::load(settings_io::defaultSettingsPath());

    state_.worldState.systems.sound.setBGMVolume(state_.settings.bgmVolume);
    state_.worldState.systems.sound.setSFXVolume(state_.settings.sfxVolume);
    state_.worldState.systems.cameraSystem.setSensitivity(state_.settings.mouseSensitivity);

    sceneRenderer_->setCullingDistance(state_.settings.drawDistance);

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

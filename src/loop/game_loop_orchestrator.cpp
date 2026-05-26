// =============================================================================
// game_loop_orchestrator.cpp — InputSystem + ActionState + LayerStack 駆動
//   top layer の mouseCapturePolicy が KeyboardMouseDevice の入力モード /
//   SDL relative mouse mode / state_.runtime.mouseCapture を駆動する。
// =============================================================================
#define NOMINMAX
#include "loop/game_loop_orchestrator.h"
#include "core/settings_io.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <memory>

#include "loop/layer.h"
#include "loop/layer_factory.h"
#include "loop/layer_stack.h"
#include "renderer/vulkan_renderer.h"
#include "systems/input_system.h"
#include "systems/keyboard_mouse_device.h"

void GameLoopOrchestrator::run(GameState& gameState, ILayerFactory& layerFactory) const {

    // ─── InputSystem setup ────────────────────────
    InputSystem inputSystem;
    inputSystem.addDevice(
        std::make_unique<KeyboardMouseDevice>(gameState.runtime.window, gameState.settings.keyMapping));

    // ─── LayerStack ─────────────────────────────
    LayerStack stack(gameState.worldState.data.vulkan, gameState);
    stack.push(layerFactory.createTitleLayer());

    bool prevMouseCapture = false;

    while (!stack.quitRequested() && !stack.empty()) {
        // ─── 1. Time ──────────────────────────
        const uint64_t now = SDL_GetTicks();
        float dt = std::min(static_cast<float>(now - gameState.runtime.lastTicks) / 1000.f, 0.05f);
        gameState.runtime.lastTicks = now;

        gameState.runtime.fpsFrames += 1;
        gameState.runtime.fpsAccum += dt;
        if (gameState.runtime.fpsAccum >= 0.5f) {
            gameState.runtime.fps = static_cast<float>(gameState.runtime.fpsFrames) / gameState.runtime.fpsAccum;
            gameState.runtime.fpsAccum = 0.f;
            gameState.runtime.fpsFrames = 0;
        }

        // ─── 2. KeyMapping dirty reflect ──────
        if (gameState.settings.keyMappingDirty) {
            if (auto* kb = inputSystem.findDeviceOfType<KeyboardMouseDevice>()) {
                kb->setMapping(gameState.settings.keyMapping);
            }
            gameState.settings.keyMappingDirty = false;
        }

        // Persist settings to disk when a layer marked them dirty (e.g. Save).
        if (gameState.settings.persistDirty) {
            settings_io::save(gameState.settings, settings_io::defaultSettingsPath());
            gameState.settings.persistDirty = false;
        }

        // ─── 3. Apply top layer's MouseCapturePolicy ──
        {
            ILayer* topLayer = stack.top();
            const bool wantCapture =
                topLayer && topLayer->mouseCapturePolicy() == MouseCapturePolicy::Locked;
            if (wantCapture != prevMouseCapture) {
                // Apply SDL relative mouse mode
                if (gameState.runtime.window) {
                    SDL_SetWindowRelativeMouseMode(gameState.runtime.window, wantCapture);
                }
                // Toggle KeyboardMouseDevice game input mode
                if (auto* kb = inputSystem.findDeviceOfType<KeyboardMouseDevice>()) {
                    kb->setGameInputMode(wantCapture);
                }
                gameState.runtime.mouseCapture = wantCapture;
                prevMouseCapture = wantCapture;
            }
        }

        // ─── 4. Poll input ────────────────────
        inputSystem.poll(dt);

        // ─── 5. handleEvents ──────────────────
        stack.handleEvents(inputSystem.events());
        stack.flushPending();

        // ─── 6. update ─────────────────────────
        stack.update(dt, inputSystem.state());
        stack.flushPending();

        // ─── 7. render ────────────────────────
        stack.render();
        stack.flushPending();
    }

    // Release mouse capture on exit
    if (gameState.runtime.window) {
        SDL_SetWindowRelativeMouseMode(gameState.runtime.window, false);
    }
}

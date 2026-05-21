// =============================================================================
// game_loop_orchestrator.cpp - InputSystem + ActionState
// + Phase 1C: LayerStack 3-arg, inputSystem.poll(dt)
// + Phase 1C fix: top layer's mouseCapturePolicy drives:
//                 - KeyboardMouseDevice::setGameInputMode
//                 - SDL relative mouse mode (cursor capture)
//                 - state_.runtime.mouseCapture flag
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
#include "scene/scene_renderer.h"
#include "systems/input_system.h"
#include "systems/keyboard_mouse_device.h"

void GameLoopOrchestrator::run(GameState& s, ILayerFactory& layerFactory,
                               SceneRenderer& sceneRenderer, float gravity, float jumpSpeed) const {
    (void)gravity;
    (void)jumpSpeed;

    // ─── InputSystem setup ────────────────────────
    InputSystem inputSystem;
    inputSystem.addDevice(
        std::make_unique<KeyboardMouseDevice>(s.runtime.window, s.settings.keyMapping));

    // ─── LayerStack ─────────────────────────────
    LayerStack stack(sceneRenderer, s.worldState.data.vulkan, s);
    stack.push(layerFactory.createTitleLayer());

    bool prevMouseCapture = false;

    while (!stack.quitRequested() && !stack.empty()) {
        // ─── 1. Time ──────────────────────────
        const uint64_t now = SDL_GetTicks();
        float dt = std::min(static_cast<float>(now - s.runtime.lastTicks) / 1000.f, 0.05f);
        s.runtime.lastTicks = now;

        s.runtime.fpsFrames += 1;
        s.runtime.fpsAccum += dt;
        if (s.runtime.fpsAccum >= 0.5f) {
            s.runtime.fps = static_cast<float>(s.runtime.fpsFrames) / s.runtime.fpsAccum;
            s.runtime.fpsAccum = 0.f;
            s.runtime.fpsFrames = 0;
        }

        // ─── 2. KeyMapping dirty reflect ──────
        if (s.settings.keyMappingDirty) {
            if (auto* kb = inputSystem.findDeviceOfType<KeyboardMouseDevice>()) {
                kb->setMapping(s.settings.keyMapping);
            }
            s.settings.keyMappingDirty = false;
        }

        // Persist settings to disk when a layer marked them dirty (e.g. Save).
        if (s.settings.persistDirty) {
            settings_io::save(s.settings, settings_io::defaultSettingsPath());
            s.settings.persistDirty = false;
        }

        // ─── 3. Apply top layer's MouseCapturePolicy ──
        {
            ILayer* topLayer = stack.top();
            const bool wantCapture =
                topLayer && topLayer->mouseCapturePolicy() == MouseCapturePolicy::Locked;
            if (wantCapture != prevMouseCapture) {
                // Apply SDL relative mouse mode
                if (s.runtime.window) {
                    SDL_SetWindowRelativeMouseMode(s.runtime.window, wantCapture);
                }
                // Toggle KeyboardMouseDevice game input mode
                if (auto* kb = inputSystem.findDeviceOfType<KeyboardMouseDevice>()) {
                    kb->setGameInputMode(wantCapture);
                }
                s.runtime.mouseCapture = wantCapture;
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
    if (s.runtime.window) {
        SDL_SetWindowRelativeMouseMode(s.runtime.window, false);
    }
}

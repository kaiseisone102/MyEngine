#include "systems/input_system.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

void InputSystem::collectEvents(SDL_Window* window, bool mouseCapture, EventBus& bus) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        const ImGuiIO& io = ImGui::GetIO();

        switch (event.type) {
            case SDL_EVENT_QUIT:
                bus.push(QuitRequested{});
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (window && event.window.windowID == SDL_GetWindowID(window))
                    bus.push(QuitRequested{});
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (window && event.window.windowID == SDL_GetWindowID(window))
                    bus.push(WindowResizeRequested{});
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!io.WantCaptureKeyboard) {
                    if (event.key.scancode == SDL_SCANCODE_ESCAPE)
                        bus.push(ToggleMouseCaptureRequested{});
                    if (event.key.scancode == SDL_SCANCODE_TAB) bus.push(ToggleCameraRequested{});
                    if (event.key.scancode == SDL_SCANCODE_SPACE) bus.push(JumpRequested{});
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!io.WantCaptureMouse) {
                    if (mouseCapture) {
                        if (event.button.button == SDL_BUTTON_LEFT) bus.push(AttackRequested{});
                        if (event.button.button == SDL_BUTTON_RIGHT)
                            bus.push(StrongAttackRequested{});
                    } else {
                        bus.push(CaptureMouseRequested{});
                    }
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (!io.WantCaptureMouse && mouseCapture) {
                    bus.push(MouseLookDelta{static_cast<float>(event.motion.xrel),
                                            static_cast<float>(event.motion.yrel)});
                }
                break;
            default:
                break;
        }
    }
}

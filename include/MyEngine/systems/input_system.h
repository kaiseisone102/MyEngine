#pragma once

#include <SDL3/SDL.h>

#include "core/event_bus.h"

class InputSystem {
   public:
    void collectEvents(SDL_Window* window, bool mouseCapture, EventBus& bus);
};

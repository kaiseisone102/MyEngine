// =============================================================================
// input_system.cpp — 実装
// =============================================================================
#include "systems/input_system.h"

#include <utility>

void InputSystem::addDevice(std::unique_ptr<IInputDevice> device) {
    if (device) {
        devices_.push_back(std::move(device));
    }
}

void InputSystem::poll(float dt) {
    events_.clear();
    state_ = ActionState{};

    for (auto& dev : devices_) {
        dev->poll(dt, events_, state_);
    }
}

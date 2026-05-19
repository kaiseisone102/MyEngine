#pragma once
// =============================================================================
// input_system.h — 入力の単一情報源
// =============================================================================

#include <memory>
#include <vector>

#include "core/action_state.h"
#include "core/event_bus.h"
#include "systems/input_device.h"

class InputSystem {
   public:
    InputSystem() = default;

    void addDevice(std::unique_ptr<IInputDevice> device);

    // 全デバイスから 1 フレーム分の入力を収集。 events_ / state_ を再構築。
    // dt: このフレームの経過時間 (秒)。 デバイス内のリピート判定等で使われる。
    void poll(float dt);

    const EventBus& events() const { return events_; }
    const ActionState& state() const { return state_; }

    template <typename T>
    T* findDeviceOfType() {
        for (auto& d : devices_) {
            if (auto* p = dynamic_cast<T*>(d.get())) return p;
        }
        return nullptr;
    }

   private:
    std::vector<std::unique_ptr<IInputDevice>> devices_;
    EventBus events_;
    ActionState state_;
};

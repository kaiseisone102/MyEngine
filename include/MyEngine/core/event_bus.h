#pragma once

#include <variant>
#include <vector>

// フレーム内イベント（意図・入力中心）
struct QuitRequested {};
struct WindowResizeRequested {};
struct ToggleMouseCaptureRequested {};
struct CaptureMouseRequested {};
struct ToggleCameraRequested {};
struct JumpRequested {};
struct AttackRequested {};        // Left_mouse_button: 薙ぎ払う攻撃
struct StrongAttackRequested {};  // Right_mouse_button: 頭上から振り下ろす強攻撃
struct MouseLookDelta {
    float xrel = 0.f;
    float yrel = 0.f;
};

using GameEvent = std::variant<QuitRequested, WindowResizeRequested, ToggleMouseCaptureRequested,
                               CaptureMouseRequested, ToggleCameraRequested, JumpRequested,
                               AttackRequested, StrongAttackRequested, MouseLookDelta>;

class EventBus {
   public:
    void push(const GameEvent& ev);
    void push(GameEvent&& ev);
    std::vector<GameEvent> drain();
    void clear();
    bool empty() const;

   private:
    std::vector<GameEvent> queue_;
};

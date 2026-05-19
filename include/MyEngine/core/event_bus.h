#pragma once
// =============================================================================
// event_bus.h — 入力意図イベント (input intents) を保持する 1 フレームの bus
// =============================================================================
// 整理:
//   - ToggleMouseCaptureRequested / CaptureMouseRequested を削除
//     (EventConsumerSystem 廃止に伴い未使用に。 マウスキャプチャは
//      orchestrator の MouseCapturePolicy 駆動に置き換え済)
//   - DebugKeyPressed を index ベース → scancode ベースに変更
//     (F6/F7/F8/F9/F10/F11/F12 を統一的に扱うため)
// =============================================================================

#include <variant>
#include <vector>

struct QuitRequested {};
struct WindowResizeRequested {};
struct ToggleCameraRequested {};
struct JumpRequested {};
struct AttackRequested {};
struct StrongAttackRequested {};
struct MouseLookDelta {
    float xrel = 0.f;
    float yrel = 0.f;
};

struct MenuNavigateUpRequested {};
struct MenuNavigateDownRequested {};
struct MenuConfirmRequested {};
struct MenuBackRequested {};
struct MenuAdjustLeftRequested {};
struct MenuAdjustRightRequested {};

// デバッグキー押下イベント。 scancode に SDL_SCANCODE_F6 などが入る。
// 受信側は `if (dk->scancode == SDL_SCANCODE_F7)` で分岐する。
struct DebugKeyPressed {
    int scancode = 0;  // SDL_Scancode を int で保持
};

// Raw 入力イベント (KeyConfigLayer 等が新しいキー割り当てを取り込むため)。
// 通常のゲームロジックは Jump/Attack 等の意図イベントを使うこと。
//   - RawKeyPressed: 任意のキー押下が発生 (scancode のみ)。
//   - RawMouseButtonPressed: 任意のマウスボタン押下 (1=Left, 2=Middle,
//     3=Right, 4=X1, 5=X2)。
struct RawKeyPressed {
    int scancode = 0;
};
struct RawMouseButtonPressed {
    int button = 0;
};

using GameEvent = std::variant<QuitRequested, WindowResizeRequested, ToggleCameraRequested,
                               JumpRequested, AttackRequested, StrongAttackRequested,
                               MouseLookDelta, MenuNavigateUpRequested,
                               MenuNavigateDownRequested, MenuConfirmRequested,
                               MenuBackRequested, MenuAdjustLeftRequested,
                               MenuAdjustRightRequested, DebugKeyPressed, RawKeyPressed,
                               RawMouseButtonPressed>;

class EventBus {
   public:
    void push(const GameEvent& ev);
    void push(GameEvent&& ev);

    const std::vector<GameEvent>& events() const { return queue_; }

    void clear();
    bool empty() const;
    std::vector<GameEvent> drain();

   private:
    std::vector<GameEvent> queue_;
};

template <typename E>
inline const E* findEvent(const EventBus& bus) {
    for (const auto& ev : bus.events()) {
        if (auto* p = std::get_if<E>(&ev)) return p;
    }
    return nullptr;
}

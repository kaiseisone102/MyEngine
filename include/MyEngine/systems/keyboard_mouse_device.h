#pragma once
// =============================================================================
// keyboard_mouse_device.h — キーボード + マウス入力デバイス
// =============================================================================
// 設計:
//   gameInputMode_ という単一フラグでデバイスの振る舞いを切り替える。
//   - true  : ゲーム入力モード (GameplayLayer 等の Locked Layer がトップ)
//             - ゲーム意図イベント (Jump/Attack/Move/Camera/MouseLook) 発行
//             - ActionState (WASD 等の連続入力) を埋める
//             - MouseLookDelta 発行
//             - メニュー系イベント (MenuBack 等) も発行 (オーバーレイ呼び出し用)
//   - false : メニュー入力モード (Released Layer がトップ)
//             - ゲーム意図イベントは発行しない
//             - ActionState は埋めない
//             - メニュー系イベント発行 (キーリピート含む)
//
//   ImGui の WantCaptureKeyboard / WantCaptureMouse は **見ない**。
//   ImGui への入力供給は ImGui_ImplSDL3_ProcessEvent で別途行う。
//   フォーカス争いを構造的に排除する。
//
// デバッグキー:
//   F6/F7/F8/F9/F10/F11/F12 すべてを固定スキャンコードでエッジ検出し、
//   DebugKeyPressed{scancode: SDL_SCANCODE_F?} イベントを発行する。
//   受信側 (GameplayLayer 等) は scancode で分岐する。
// =============================================================================

#include <SDL3/SDL.h>

#include <array>
#include <vector>

#include "core/key_mapping.h"
#include "systems/input_device.h"

class EventBus;
struct ActionState;
struct InputBinding;

class KeyboardMouseDevice : public IInputDevice {
   public:
    KeyboardMouseDevice(SDL_Window* window, KeyMapping mapping);
    ~KeyboardMouseDevice() override = default;

    void poll(float dt, EventBus& events, ActionState& state) override;

    void setMapping(const KeyMapping& m);

    void setGameInputMode(bool on) { gameInputMode_ = on; }

   private:
    enum class NavKind { Up, Down, Left, Right };

    struct RepeatBinding {
        SDL_Scancode scancode;
        NavKind kind;
    };
    struct RepeatState {
        bool prevHeld = false;
        float holdTime = 0.f;
        float lastFireTime = 0.f;
    };

    void rebuildRepeatBindings();
    bool isBindingHeld(const InputBinding& b, const bool* keys, Uint32 mouseState) const;

    SDL_Window* window_ = nullptr;
    KeyMapping mapping_;
    bool gameInputMode_ = false;

    // F6 ~ F12 のエッジ検出用 (合計 7 キー)。
    // インデックスは kDebugScancodes と対応。
    static constexpr int kDebugKeyCount = 7;
    static constexpr std::array<SDL_Scancode, kDebugKeyCount> kDebugScancodes = {
        SDL_SCANCODE_F6,  SDL_SCANCODE_F7,  SDL_SCANCODE_F8, SDL_SCANCODE_F9,
        SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12,
    };
    std::array<bool, kDebugKeyCount> prevDebug_{};

    std::vector<RepeatBinding> repeatBindings_;
    std::vector<RepeatState>   repeatStates_;

    static constexpr float kInitialDelay   = 0.40f;
    static constexpr float kRepeatInterval = 0.08f;
};

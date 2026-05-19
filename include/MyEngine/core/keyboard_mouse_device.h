#pragma once
// =============================================================================
// keyboard_mouse_device.h — キーボード + マウスの入力状態抽象化
// =============================================================================
// SDL のグローバル状態をラップ。 KeyMapping を適用して GameAction ベースで状態取得。
// edge detection (just pressed / just released) も提供。
// =============================================================================

#include <SDL3/SDL.h>

#include <array>

#include "core/key_mapping.h"

class KeyboardMouseDevice {
   public:
    void poll();  // 毎フレ始めに呼ぶ
    void applyMapping(const KeyMapping& mapping) { mapping_ = mapping; }

    // 連続押下
    bool isDown(GameAction action) const;
    // 押下瞬間
    bool justPressed(GameAction action) const;
    // 離れた瞬間
    bool justReleased(GameAction action) const;

    // マウス相対移動 (フレーム間)
    float mouseDx() const { return mouseDx_; }
    float mouseDy() const { return mouseDy_; }

   private:
    KeyMapping mapping_;
    std::array<bool, static_cast<int>(GameAction::Count)> curr_{};
    std::array<bool, static_cast<int>(GameAction::Count)> prev_{};
    float mouseDx_ = 0.f;
    float mouseDy_ = 0.f;
};

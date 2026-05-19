#pragma once
// =============================================================================
// input_binding.h — キー or マウスボタンを統一して表現するバインド型
// =============================================================================

#include <SDL3/SDL.h>

#include <string>

struct InputBinding {
    enum class Source {
        Keyboard,
        Mouse,
    };

    Source source = Source::Keyboard;
    int code = 0;

    constexpr bool operator==(const InputBinding& o) const {
        return source == o.source && code == o.code;
    }
    constexpr bool operator!=(const InputBinding& o) const { return !(*this == o); }

    constexpr bool isAssigned() const { return code != 0; }

    static constexpr InputBinding key(SDL_Scancode s) {
        return InputBinding{Source::Keyboard, static_cast<int>(s)};
    }
    static constexpr InputBinding mouse(Uint8 button) {
        return InputBinding{Source::Mouse, static_cast<int>(button)};
    }
    static constexpr InputBinding none() {
        return InputBinding{Source::Keyboard, 0};
    }

    // フル名 (キーコンフィグ画面表示用): "W", "Left Shift", "Mouse Left" 等
    std::string displayName() const;

    // 短縮名 (デバッグオーバーレイ等の狭い場所用): "W", "LShft", "LMB" 等
    // 6 文字以内を目安。
    std::string shortName() const;
};

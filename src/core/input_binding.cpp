// =============================================================================
// input_binding.cpp — displayName + shortName 実装
// =============================================================================
#include "core/input_binding.h"

#include <cstring>

std::string InputBinding::displayName() const {
    if (!isAssigned()) return "---";

    switch (source) {
        case Source::Keyboard: {
            const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
            if (name && name[0] != '\0') return name;
            return "Unknown Key";
        }
        case Source::Mouse: {
            switch (code) {
                case SDL_BUTTON_LEFT:   return "Mouse Left";
                case SDL_BUTTON_RIGHT:  return "Mouse Right";
                case SDL_BUTTON_MIDDLE: return "Mouse Middle";
                case SDL_BUTTON_X1:     return "Mouse X1";
                case SDL_BUTTON_X2:     return "Mouse X2";
                default:                return "Mouse ?";
            }
        }
    }
    return "?";
}

std::string InputBinding::shortName() const {
    if (!isAssigned()) return "-";

    switch (source) {
        case Source::Mouse: {
            switch (code) {
                case SDL_BUTTON_LEFT:   return "LMB";
                case SDL_BUTTON_RIGHT:  return "RMB";
                case SDL_BUTTON_MIDDLE: return "MMB";
                case SDL_BUTTON_X1:     return "MX1";
                case SDL_BUTTON_X2:     return "MX2";
                default:                return "M?";
            }
        }
        case Source::Keyboard: {
            // 特定 scancode の短縮表現
            const auto sc = static_cast<SDL_Scancode>(code);
            switch (sc) {
                case SDL_SCANCODE_LSHIFT: return "LShft";
                case SDL_SCANCODE_RSHIFT: return "RShft";
                case SDL_SCANCODE_LCTRL:  return "LCtrl";
                case SDL_SCANCODE_RCTRL:  return "RCtrl";
                case SDL_SCANCODE_LALT:   return "LAlt";
                case SDL_SCANCODE_RALT:   return "RAlt";
                case SDL_SCANCODE_SPACE:  return "Space";
                case SDL_SCANCODE_RETURN: return "Enter";
                case SDL_SCANCODE_ESCAPE: return "Esc";
                case SDL_SCANCODE_TAB:    return "Tab";
                case SDL_SCANCODE_UP:     return "Up";
                case SDL_SCANCODE_DOWN:   return "Down";
                case SDL_SCANCODE_LEFT:   return "Left";
                case SDL_SCANCODE_RIGHT:  return "Right";
                default:
                    break;
            }
            // それ以外は displayName を返す (大半は 1 文字 "W" 等で十分短い)
            const char* name = SDL_GetScancodeName(sc);
            if (!name || name[0] == '\0') return "?";
            // 6 文字超なら先頭 5 文字に丸める
            const size_t len = std::strlen(name);
            if (len > 6) {
                return std::string(name, 5);
            }
            return name;
        }
    }
    return "?";
}

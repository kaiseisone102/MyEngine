// =============================================================================
// keyboard_mouse_device.cpp — gameInputMode 一本化版
// デバッグキー F6/F7/F8/F9/F10/F11/F12 を統一的に固定スキャンコードで処理。
// DebugKeyPressed には scancode をそのまま入れる。
//
// guard 入力対応:
//   mapping_.guard が押されている間 state.guardHeld = true。
//   GameplayLayer がこれを CShield::guarding に反映する。
// =============================================================================
#include "systems/keyboard_mouse_device.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include "core/action_state.h"
#include "core/event_bus.h"
#include <iostream>

KeyboardMouseDevice::KeyboardMouseDevice(SDL_Window* window, KeyMapping mapping)
    : window_(window), mapping_(mapping) {
    rebuildRepeatBindings();
}

void KeyboardMouseDevice::setMapping(const KeyMapping& m) {
    mapping_ = m;
    rebuildRepeatBindings();
}

void KeyboardMouseDevice::rebuildRepeatBindings() {
    repeatBindings_.clear();

    repeatBindings_.push_back({mapping_.menuUp,    NavKind::Up});
    repeatBindings_.push_back({mapping_.menuDown,  NavKind::Down});
    repeatBindings_.push_back({mapping_.menuLeft,  NavKind::Left});
    repeatBindings_.push_back({mapping_.menuRight, NavKind::Right});

    auto addIfKeyboard = [&](const InputBinding& b, NavKind k) {
        if (b.source == InputBinding::Source::Keyboard && b.isAssigned()) {
            repeatBindings_.push_back({static_cast<SDL_Scancode>(b.code), k});
        }
    };
    addIfKeyboard(mapping_.moveForward, NavKind::Up);
    addIfKeyboard(mapping_.moveBack,    NavKind::Down);
    addIfKeyboard(mapping_.moveLeft,    NavKind::Left);
    addIfKeyboard(mapping_.moveRight,   NavKind::Right);

    repeatStates_.assign(repeatBindings_.size(), RepeatState{});
}

bool KeyboardMouseDevice::isBindingHeld(const InputBinding& b, const bool* keys,
                                          Uint32 mouseState) const {
    if (!b.isAssigned()) return false;
    if (b.source == InputBinding::Source::Keyboard) {
        return keys[b.code];
    } else {
        return (mouseState & SDL_BUTTON_MASK(b.code)) != 0;
    }
}

void KeyboardMouseDevice::poll(float dt, EventBus& events, ActionState& state) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type) {
            case SDL_EVENT_QUIT:
                events.push(QuitRequested{});
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (window_ && event.window.windowID == SDL_GetWindowID(window_)) {
                    events.push(QuitRequested{});
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (window_ && event.window.windowID == SDL_GetWindowID(window_)) {
                    events.push(WindowResizeRequested{});
                }
                break;

            case SDL_EVENT_KEY_DOWN: {
                const SDL_Scancode sc = event.key.scancode;

                events.push(RawKeyPressed{static_cast<int>(sc)});

                if (sc == mapping_.menuBack) events.push(MenuBackRequested{});
                if (sc == mapping_.menuConfirm || sc == mapping_.menuConfirmAlt) {
                    events.push(MenuConfirmRequested{});
                }

                if (gameInputMode_) {
                    const auto kb = InputBinding::key(sc);
                    if (kb == mapping_.jump)         events.push(JumpRequested{});
                    if (kb == mapping_.toggleCamera) events.push(ToggleCameraRequested{});
                    if (kb == mapping_.attack)       events.push(AttackRequested{});
                    if (kb == mapping_.strongAttack) events.push(StrongAttackRequested{});
                }
                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                const Uint8 btn = event.button.button;

                events.push(RawMouseButtonPressed{static_cast<int>(btn)});

                if (gameInputMode_) {
                    const auto mb = InputBinding::mouse(btn);
                    if (mb == mapping_.attack)       events.push(AttackRequested{});
                    if (mb == mapping_.strongAttack) events.push(StrongAttackRequested{});
                    if (mb == mapping_.jump)         events.push(JumpRequested{});
                    if (mb == mapping_.toggleCamera) events.push(ToggleCameraRequested{});
                }
                break;
            }

            case SDL_EVENT_MOUSE_MOTION: {
                if (!gameInputMode_) break;
                events.push(MouseLookDelta{static_cast<float>(event.motion.xrel),
                                            static_cast<float>(event.motion.yrel)});
                state.lookX += static_cast<float>(event.motion.xrel);
                state.lookY += static_cast<float>(event.motion.yrel);
                break;
            }

            default:
                break;
        }
    }

    const bool* keys = SDL_GetKeyboardState(nullptr);
    const Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);

    if (gameInputMode_) {
        float mx = 0.f;
        float mz = 0.f;
        auto applyAxis = [&](const InputBinding& neg, const InputBinding& pos, float& axis) {
            if (neg.source == InputBinding::Source::Keyboard && neg.isAssigned() && keys[neg.code])
                axis -= 1.f;
            if (pos.source == InputBinding::Source::Keyboard && pos.isAssigned() && keys[pos.code])
                axis += 1.f;
        };
        applyAxis(mapping_.moveLeft, mapping_.moveRight,   mx);
        applyAxis(mapping_.moveBack, mapping_.moveForward, mz);

        state.moveX += mx;
        state.moveZ += mz;

        if (mapping_.moveUp.source == InputBinding::Source::Keyboard &&
            mapping_.moveUp.isAssigned() && keys[mapping_.moveUp.code]) {
            state.moveUp = true;
        }
        if (mapping_.moveDown.source == InputBinding::Source::Keyboard &&
            mapping_.moveDown.isAssigned() && keys[mapping_.moveDown.code]) {
            state.moveDown = true;
        }

        if (isBindingHeld(mapping_.sprint, keys, mouseState)) state.sprint = true;
        if (isBindingHeld(mapping_.crouch, keys, mouseState)) state.crouch = true;

        if (isBindingHeld(mapping_.jump,   keys, mouseState)) state.jumpHeld   = true;
        if (isBindingHeld(mapping_.attack, keys, mouseState)) state.attackHeld = true;
        // ガード状態 (LCtrl デフォルト)
        if (isBindingHeld(mapping_.guard,  keys, mouseState)) state.guardHeld  = true;
    }

    // ─── デバッグキー (F6 ~ F12) のエッジ検出 ─────────────────
    if (gameInputMode_) {
        for (int i = 0; i < kDebugKeyCount; ++i) {
            const SDL_Scancode sc = kDebugScancodes[i];
            const bool now = keys[sc];
            if (now && !prevDebug_[i]) {
                std::cout<<"[DEBUG] F-key push: sc="<<int(sc)<<" gameInputMode="<<gameInputMode_<<"\n";
                events.push(DebugKeyPressed{static_cast<int>(sc)});
            }
            prevDebug_[i] = now;
        }
    } else {
        for (int i = 0; i < kDebugKeyCount; ++i) prevDebug_[i] = false;
    }

    for (size_t i = 0; i < repeatBindings_.size(); ++i) {
        const auto& rb = repeatBindings_[i];
        auto& st = repeatStates_[i];

        const bool held = keys[rb.scancode];

        bool fire = false;
        if (held && !st.prevHeld) {
            fire = true;
            st.holdTime = 0.f;
            st.lastFireTime = 0.f;
        } else if (held) {
            st.holdTime += dt;
            if (st.holdTime >= kInitialDelay) {
                if (st.holdTime - st.lastFireTime >= kRepeatInterval) {
                    fire = true;
                    st.lastFireTime = st.holdTime;
                }
            }
        } else {
            st.holdTime = 0.f;
            st.lastFireTime = 0.f;
        }
        st.prevHeld = held;

        if (fire && !gameInputMode_) {
            switch (rb.kind) {
                case NavKind::Up:    events.push(MenuNavigateUpRequested{});    break;
                case NavKind::Down:  events.push(MenuNavigateDownRequested{});  break;
                case NavKind::Left:  events.push(MenuAdjustLeftRequested{});    break;
                case NavKind::Right: events.push(MenuAdjustRightRequested{});   break;
            }
        }
    }

    if (state.moveX < -1.f) state.moveX = -1.f;
    if (state.moveX >  1.f) state.moveX =  1.f;
    if (state.moveZ < -1.f) state.moveZ = -1.f;
    if (state.moveZ >  1.f) state.moveZ =  1.f;
}

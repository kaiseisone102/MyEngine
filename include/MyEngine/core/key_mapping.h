#pragma once
// =============================================================================
// key_mapping.h — キーボード/マウス入力のマッピング設定
// =============================================================================
// バインド対象アクションは InputBinding (キーまたはマウスボタン) で保持。
//
// メニュー操作キー (menuConfirm, menuBack, menuUp/Down/Left/Right) は
// ユーザーが誤って変更すると操作不能になりやすいため、 バインド対象外
// (SDL_Scancode で固定) とする。
//
// デバッグキー (F6/F7/F8/F9/F10/F11/F12) は KeyMapping に含めない。
// keyboard_mouse_device.cpp 内で固定スキャンコードとして直接処理し、
// DebugKeyPressed イベントの scancode フィールドにそのまま流す。
// =============================================================================

#include <SDL3/SDL.h>

#include "core/input_binding.h"

struct KeyMapping {
    // ─── バインド可能 (KeyConfigLayer で変更できる) ──────────
    // 移動
    InputBinding moveForward = InputBinding::key(SDL_SCANCODE_W);
    InputBinding moveBack    = InputBinding::key(SDL_SCANCODE_S);
    InputBinding moveLeft    = InputBinding::key(SDL_SCANCODE_A);
    InputBinding moveRight   = InputBinding::key(SDL_SCANCODE_D);
    InputBinding moveUp      = InputBinding::key(SDL_SCANCODE_E);  // FPS カメラ用
    InputBinding moveDown    = InputBinding::key(SDL_SCANCODE_Q);

    // モディファイア
    InputBinding sprint = InputBinding::key(SDL_SCANCODE_LSHIFT);
    InputBinding crouch = InputBinding::key(SDL_SCANCODE_LCTRL);

    // ゲーム単発アクション
    InputBinding jump         = InputBinding::key(SDL_SCANCODE_SPACE);
    InputBinding toggleCamera = InputBinding::key(SDL_SCANCODE_TAB);
    InputBinding attack       = InputBinding::mouse(SDL_BUTTON_LEFT);
    InputBinding strongAttack = InputBinding::mouse(SDL_BUTTON_RIGHT);

    // ガード (盾構え) - LCtrl デフォルト。 crouch と被るが、 現状 crouch は未使用なので OK。
    // ガード中: 移動不可 (向き変更のみ)、 ダメージ無効化 + 盾 durability -1。
    InputBinding guard = InputBinding::key(SDL_SCANCODE_LCTRL);

    // ─── バインド対象外 (固定、 メニュー操作の安全性のため) ──
    SDL_Scancode menuConfirm    = SDL_SCANCODE_RETURN;
    SDL_Scancode menuConfirmAlt = SDL_SCANCODE_KP_ENTER;
    SDL_Scancode menuBack       = SDL_SCANCODE_ESCAPE;
    SDL_Scancode menuUp         = SDL_SCANCODE_UP;
    SDL_Scancode menuDown       = SDL_SCANCODE_DOWN;
    SDL_Scancode menuLeft       = SDL_SCANCODE_LEFT;
    SDL_Scancode menuRight      = SDL_SCANCODE_RIGHT;
};

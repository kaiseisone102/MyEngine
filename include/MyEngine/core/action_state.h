#pragma once
// =============================================================================
// action_state.h — 抽象化された連続入力状態 (continuous input state)
// =============================================================================
// 役割:
//   1 フレーム時点での「現在押されている / 入力されている」 連続入力を
//   ゲームの意図ベースで表現する。 SDL_Scancode への直接依存をなくし、
//   キーマッピング変更・コントローラー対応・テスト容易性を実現する。
//
// 単発イベント (JumpRequested, AttackRequested 等) は EventBus に残す。
// ActionState は **連続状態 (state)** 専用。
//
// jumpHeld 等の hold 状態:
//   「ボタンが押されている状態」 を表現する。 イベント (Jump"Requested") と
//   重複するが、 用途が違う:
//     - JumpRequested : 「押した瞬間」 (エッジ検出済み、 1 フレームだけ true)
//     - jumpHeld      : 「押されている間ずっと true」 (連続)
//   例: Land アニメを「ジャンプキー連打でキャンセル」 する場合は jumpHeld。
// =============================================================================

struct ActionState {
    // ─── 移動 (TPS Player / FPS Camera 共通) ──────────────────────
    float moveX = 0.f;
    float moveZ = 0.f;

    // ─── FPS カメラ専用の上下移動 (Q/E) ─────────────────────────
    bool moveUp   = false;
    bool moveDown = false;

    // ─── マウス/右スティック (look) ────────────────────────────
    float lookX = 0.f;
    float lookY = 0.f;

    // ─── モディファイア ──────────────────────────────────────
    bool sprint = false;
    bool crouch = false;

    // ─── アクション hold 状態 ─────────────────────────────────
    bool jumpHeld   = false;
    bool attackHeld = false;
    // ガードキー (LCtrl デフォルト) 押下中。 GameplayLayer が
    // CShield::guarding に反映する。 盾未装備時は反映されても無効。
    bool guardHeld  = false;
};

#pragma once

#include <flecs.h>

#include "core/components.h"
#include "systems/sound_manager.h"

class AudioEventSystem {
   public:
    // プレイヤー初期化直後に現在の接地状態を同期する
    void syncGroundState(flecs::entity player);

    // ジャンプ要求時のSE処理（地上時のみ）
    void onJumpRequested(flecs::entity player, bool inputLocked, SoundManager& sound) const;

    // 物理更新後に呼び、着地遷移を検出してSE再生する
    void onPostPhysics(flecs::entity player, SoundManager& sound);

   private:
    bool wasOnGround_ = false;
};

#pragma once
// =============================================================================
// hud_system.h — flecs world から HUD 描画リストを構築
// =============================================================================
// プレイヤーの HP / シールド / グリップ耐久 / 鍵カウント / 所持金 / ポーション数
// を HudDrawList に書き込む。 ダメージ時の遅延赤バー、 拾い時の緑フラッシュも処理。
// =============================================================================

#include "renderer/hud_draw_list.h"

struct WorldData;

class HudSystem {
   public:
    void update(WorldData& wd, float dt);
    void build(const WorldData& wd, float screenW, float screenH, HudDrawList& out);

   private:
    // 遅延赤バー用
    float delayedHpFloat_ = 3.f;
    float pickupGreenFlash_ = 0.f;
};

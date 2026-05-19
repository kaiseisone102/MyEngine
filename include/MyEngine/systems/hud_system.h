#pragma once
// =============================================================================
// hud_system.h — HUD ロジックの関数群
// =============================================================================

#include <glm/glm.hpp>

#include "core/components.h"

class HudDrawList;

namespace hud_system {

void drawHealthBar(HudDrawList& drawList, const CHealth& hp, float originX, float originY);

void drawShieldGauge(HudDrawList& drawList, const CShield& shield, float originX, float originY);

// Grip ゲージ:
//   金縁 + こげ茶背景の円、 中央に Grip 装着時はタイプ色の丸、 非装着時は緑の丸。
//   centerX/centerY は円の中心座標 (px、 ウィンドウ左上原点)。
void drawGripGauge(HudDrawList& drawList, const CGrip& grip, float centerX, float centerY);

// Grip ゲージの外半径を取得 (画面端からの余白計算に使う)。
// hud_system.cpp の kGripScale を変えると自動的にこの値も変わる。
float gripGaugeRadius();

}  // namespace hud_system

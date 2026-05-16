// =============================================================================
// hud_system.cpp — HUD ロジック実装 (+ Grip ゲージ スケール変数化)
// =============================================================================
// Grip ゲージのサイズは kGripScale 一発で全体を比例的に調整できる。
// 1.0 = 元のサイズ (半径 28px)、 1.5 = 1.5 倍 (半径 42px)。
// =============================================================================
#include "systems/hud_system.h"

#include <cmath>

#include "core/grip.h"
#include "renderer/hud_draw_list.h"

namespace hud_system {

namespace {

constexpr float kBarWidth = 110.f;
constexpr float kBarHeight = 18.f;
constexpr float kBarGap = 6.f;
constexpr float kFrameThick = 2.f;

constexpr glm::vec4 kFillColor{0.30f, 0.85f, 0.30f, 1.0f};
constexpr glm::vec4 kEmptyColor{0.15f, 0.15f, 0.15f, 0.85f};
constexpr glm::vec4 kFrameColor{0.0f, 0.0f, 0.0f, 1.0f};

constexpr int kShieldPointsPerGroup = 5;
constexpr float kShieldPointInnerGap = 1.f;
constexpr float kShieldHeight = 18.f;
constexpr float kShieldFrameThick = 2.f;
constexpr float kShieldInnerPad = 2.f;
constexpr float kShieldPointWidth =
    (kBarWidth - (kShieldPointsPerGroup - 1) * kShieldPointInnerGap) / kShieldPointsPerGroup;

constexpr glm::vec4 kShieldBgColor{0.15f, 0.15f, 0.20f, 0.85f};
constexpr glm::vec4 kShieldPointColor{0.20f, 0.45f, 0.95f, 1.0f};
constexpr glm::vec4 kShieldPointGuarding{0.40f, 0.70f, 1.00f, 1.0f};
constexpr glm::vec4 kShieldFrameColor{0.0f, 0.0f, 0.0f, 1.0f};

// ─── Grip ゲージ ────────────────────────────────────────
// 全サイズを比例的に変えたいときは kGripScale だけ変更すればよい。
// 1.0 = 基準サイズ (半径 28px)、 1.5 = 1.5 倍 (半径 42px)。
//
// 注: gameplay_layer.cpp 側で「ゲージの中心座標」 も同じ倍率で右にずらす
// 必要がある場合がある (画面端からはみ出ないように)。
// gripGaugeRadius() 関数で外径を公開しているので、 呼び出し側はこれを使う。
constexpr float kGripScale = 1.5f;

// 基準サイズ (kGripScale=1.0 のときの値、 編集しないこと)
constexpr float kGripBaseOuterRadius = 28.f;
constexpr float kGripBaseFrameThick = 3.f;
constexpr float kGripBaseTickOuter = 24.f;
constexpr float kGripBaseTickInner = 17.f;
constexpr float kGripBaseOrbRadius = 14.f;

// スケール適用後の実値 (描画で使う)
constexpr float kGripOuterRadius = kGripBaseOuterRadius * kGripScale;
constexpr float kGripFrameThick = kGripBaseFrameThick * kGripScale;
constexpr float kGripTickOuter = kGripBaseTickOuter * kGripScale;
constexpr float kGripTickInner = kGripBaseTickInner * kGripScale;
constexpr float kGripOrbRadius = kGripBaseOrbRadius * kGripScale;

// 隙間角度はスケール不変 (角度なので px ではない)
constexpr float kGripTickGapDeg = 1.0f;

constexpr glm::vec4 kGripFrameColor{0.95f, 0.78f, 0.25f, 1.0f};
constexpr glm::vec4 kGripBgColor{0.20f, 0.12f, 0.06f, 0.95f};
constexpr glm::vec4 kGripOrbNone{0.30f, 0.95f, 0.40f, 1.0f};

constexpr glm::vec4 kGripTickRegular{0.95f, 0.20f, 0.10f, 1.0f};
constexpr glm::vec4 kGripTickSection{1.00f, 0.90f, 0.30f, 1.0f};
constexpr int kGripTotalTicks = 32;

constexpr float kPI = 3.14159265358979323846f;

}  // namespace

float gripGaugeRadius() { return kGripOuterRadius; }

void drawHealthBar(HudDrawList& drawList, const CHealth& hp, float originX, float originY) {
    for (int i = 0; i < hp.segmentCount; ++i) {
        const float x = originX + i * (kBarWidth + kBarGap);
        const float y = originY;

        drawList.addRectFilled({x, y}, {kBarWidth, kBarHeight}, kEmptyColor);

        const bool isCurrentSegment = (i == hp.segmentCount - 1);
        const float fillRatio = isCurrentSegment ? static_cast<float>(hp.currentHp) /
                                                       static_cast<float>(CHealth::kSegmentSize)
                                                 : 1.f;
        if (fillRatio > 0.f) {
            const float fillW = kBarWidth * fillRatio;
            drawList.addRectFilled({x, y}, {fillW, kBarHeight}, kFillColor);
        }

        drawList.addRectOutline({x, y}, {kBarWidth, kBarHeight}, kFrameColor, kFrameThick);
    }
}

void drawShieldGauge(HudDrawList& drawList, const CShield& shield, float originX, float originY) {
    if (shield.type == ShieldType::None) return;

    const int maxDur = CShield::maxDurability(shield.type);
    if (maxDur <= 0) return;

    const int groupCount = (maxDur + kShieldPointsPerGroup - 1) / kShieldPointsPerGroup;
    const glm::vec4& pointColor = shield.guarding ? kShieldPointGuarding : kShieldPointColor;

    for (int g = 0; g < groupCount; ++g) {
        const float groupX = originX + g * (kBarWidth + kBarGap);
        const float groupY = originY;

        drawList.addRectFilled({groupX, groupY}, {kBarWidth, kShieldHeight}, kShieldBgColor);

        const int pointStart = g * kShieldPointsPerGroup;
        const int pointEndInGroup = pointStart + kShieldPointsPerGroup;
        const int liveEnd =
            (shield.durability < pointEndInGroup) ? shield.durability : pointEndInGroup;

        const float innerY = groupY + kShieldInnerPad;
        const float innerH = kShieldHeight - kShieldInnerPad * 2.f;

        for (int i = pointStart; i < liveEnd; ++i) {
            const int idxInGroup = i - pointStart;
            const float x = groupX + idxInGroup * (kShieldPointWidth + kShieldPointInnerGap);
            drawList.addRectFilled({x, innerY}, {kShieldPointWidth, innerH}, pointColor);
        }

        drawList.addRectOutline({groupX, groupY}, {kBarWidth, kShieldHeight}, kShieldFrameColor,
                                kShieldFrameThick);
    }
}

void drawGripGauge(HudDrawList& drawList, const CGrip& grip, float centerX, float centerY) {
    const glm::vec2 center{centerX, centerY};

    drawList.addCircle(center, kGripOuterRadius, kGripBgColor, /*gloss=*/false);

    if (grip.active()) {
        const float segDeg = 360.f / static_cast<float>(kGripTotalTicks);
        const float segRad = segDeg * (kPI / 180.f);
        const float gapRad = kGripTickGapDeg * (kPI / 180.f);
        const float halfSeg = segRad * 0.5f;

        const int consumed = grip::def(grip.type).maxDurability - grip.durability;

        for (int i = 0; i < kGripTotalTicks; ++i) {
            const bool consumedThis = (i == 0) ? (consumed >= kGripTotalTicks) : (consumed >= i);
            if (consumedThis) continue;

            const float centerAng = static_cast<float>(i) * segRad;
            float angStart = centerAng - halfSeg + gapRad * 0.5f;
            float angEnd = centerAng + halfSeg - gapRad * 0.5f;

            if (angStart < 0.f) angStart += 2.f * kPI;
            if (angEnd < 0.f) angEnd += 2.f * kPI;

            const bool isSection = (i % 8) == 0;
            const glm::vec4& tickColor = isSection ? kGripTickSection : kGripTickRegular;

            drawList.addCircleSegment(center, kGripTickOuter, kGripTickInner, angStart, angEnd,
                                      tickColor);
        }
    }

    drawList.addRing(center, kGripOuterRadius, kGripOuterRadius - kGripFrameThick, kGripFrameColor);

    const glm::vec4 orbColor = grip.active() ? grip::def(grip.type).uiOrbColor : kGripOrbNone;
    drawList.addCircle(center, kGripOrbRadius, orbColor, /*gloss=*/true,
                       /*glossStrength=*/0.6f);
}

}  // namespace hud_system

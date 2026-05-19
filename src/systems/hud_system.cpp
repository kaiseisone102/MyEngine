// =============================================================================
// hud_system.cpp — barSegs を segmentCount ベースに変更
// =============================================================================
// 仕様:
//   - HP 0 になった区画は消える (= バーが物理的に短くなる)
//   - 死亡 (segmentCount=0) しても 1 セグメント分の空バーは残す
// =============================================================================
#include "systems/hud_system.h"

#include <algorithm>
#include <cmath>

#include "core/grip.h"
#include "renderer/hud_draw_list.h"

namespace hud_system {

namespace {

constexpr float kBarSegmentWidth = 110.f;
constexpr float kBarHeight       = 22.f;
constexpr float kBarFrameThick   = 4.f;
constexpr float kBarInnerPad     = 3.f;

constexpr float kBarEndCapRadius = 14.f;
constexpr float kBarMidRivetRadius = 4.5f;

constexpr glm::vec4 kFrameColor    {0.85f, 0.65f, 0.20f, 1.0f};
constexpr glm::vec4 kInnerBgColor  {0.05f, 0.03f, 0.02f, 0.98f};
constexpr glm::vec4 kEndCapColor   {0.85f, 0.65f, 0.20f, 1.0f};
constexpr glm::vec4 kMidRivetColor {0.95f, 0.78f, 0.30f, 1.0f};

constexpr glm::vec4 kHpFillColor       {0.20f, 0.50f, 0.20f, 1.0f};
constexpr glm::vec4 kHpFillFlashPeak   {0.85f, 1.00f, 0.75f, 1.0f};

// ダメージ赤バー (kFlagBarFlat でベタ塗り、 鮮やか純赤)
constexpr glm::vec4 kHpDamageColor     {1.0f, 0.0f, 0.0f, 1.0f};

constexpr glm::vec4 kShieldFillColor      {0.20f, 0.40f, 0.60f, 1.0f};
constexpr glm::vec4 kShieldGuardingColor  {0.40f, 0.60f, 0.85f, 1.0f};

constexpr int kShieldPointsPerGroup = 5;

constexpr float kGripScale = 1.5f;
constexpr float kGripBaseOuterRadius = 28.f;
constexpr float kGripBaseFrameThick  = 3.f;
constexpr float kGripBaseTickOuter   = 24.f;
constexpr float kGripBaseTickInner   = 17.f;
constexpr float kGripBaseOrbRadius   = 14.f;
constexpr float kGripOuterRadius = kGripBaseOuterRadius * kGripScale;
constexpr float kGripFrameThick  = kGripBaseFrameThick  * kGripScale;
constexpr float kGripTickOuter   = kGripBaseTickOuter   * kGripScale;
constexpr float kGripTickInner   = kGripBaseTickInner   * kGripScale;
constexpr float kGripOrbRadius   = kGripBaseOrbRadius   * kGripScale;
constexpr float kGripTickGapDeg  = 1.0f;
constexpr glm::vec4 kGripFrameColor   {0.95f, 0.78f, 0.25f, 1.0f};
constexpr glm::vec4 kGripBgColor      {0.05f, 0.03f, 0.02f, 0.98f};
constexpr glm::vec4 kGripOrbNone      {0.30f, 0.95f, 0.40f, 1.0f};
constexpr glm::vec4 kGripTickRegular  {0.95f, 0.20f, 0.10f, 1.0f};
constexpr glm::vec4 kGripTickSection  {1.00f, 0.90f, 0.30f, 1.0f};
constexpr int kGripTotalTicks         = 32;
constexpr float kPI = 3.14159265358979323846f;

struct FillSplit {
    int fullSegs;
    float tailFillRatio;
};

FillSplit splitTotalHp(float totalHp, int segCount, int segmentSize) {
    FillSplit r;
    if (totalHp <= 0.f || segCount <= 0) {
        r.fullSegs = 0;
        r.tailFillRatio = 0.f;
        return r;
    }
    const float segSizeF = static_cast<float>(segmentSize);
    const float maxTotal = static_cast<float>(segCount) * segSizeF;
    const float clamped = std::min(totalHp, maxTotal);

    const int full = static_cast<int>(clamped / segSizeF);
    const float tailExtra = clamped - static_cast<float>(full) * segSizeF;

    if (full >= segCount) {
        r.fullSegs = segCount - 1;
        r.tailFillRatio = 1.f;
    } else {
        r.fullSegs = full;
        r.tailFillRatio = tailExtra / segSizeF;
    }
    return r;
}

void drawMechanicalBar(HudDrawList& drawList, float originX, float originY,
                        int segCount, int fullSegs, float tailFillRatio,
                        const glm::vec4& fillColor) {
    if (segCount <= 0) return;

    const float totalWidth = kBarSegmentWidth * static_cast<float>(segCount);

    drawList.addRectFilled({originX, originY}, {totalWidth, kBarHeight}, kInnerBgColor);

    drawList.addMetalFrame({originX, originY}, {totalWidth, kBarHeight}, kFrameColor,
                             kBarFrameThick / std::min(totalWidth, kBarHeight));

    const glm::vec2 fillMin{originX + kBarInnerPad, originY + kBarInnerPad};
    const glm::vec2 fillSize{totalWidth - kBarInnerPad * 2.f, kBarHeight - kBarInnerPad * 2.f};
    drawList.addBarFillSegmented(fillMin, fillSize, fillColor, segCount, fullSegs,
                                   tailFillRatio);

    const float midY = originY + kBarHeight * 0.5f;
    for (int i = 1; i < segCount; ++i) {
        const float x = originX + kBarSegmentWidth * static_cast<float>(i);
        drawList.addRivet({x, midY}, kBarMidRivetRadius, kMidRivetColor);
    }
    drawList.addRivet({originX, midY}, kBarEndCapRadius, kEndCapColor);
    drawList.addRivet({originX + totalWidth, midY}, kBarEndCapRadius, kEndCapColor);
}

void drawHpBar(HudDrawList& drawList, float originX, float originY,
                int barSegs, int currentSegmentIdx, int currentHp,
                float currentTotal, float displayedTotal,
                const glm::vec4& greenColor, const glm::vec4& flashColor,
                bool drawFlashOverlay) {
    if (barSegs <= 0) return;
    const float totalWidth = kBarSegmentWidth * static_cast<float>(barSegs);

    drawList.addRectFilled({originX, originY}, {totalWidth, kBarHeight}, kInnerBgColor);

    drawList.addMetalFrame({originX, originY}, {totalWidth, kBarHeight}, kFrameColor,
                             kBarFrameThick / std::min(totalWidth, kBarHeight));

    const glm::vec2 fillMin{originX + kBarInnerPad, originY + kBarInnerPad};
    const glm::vec2 fillSize{totalWidth - kBarInnerPad * 2.f, kBarHeight - kBarInnerPad * 2.f};

    // 赤 fill: flat=true でベタ塗り、 鮮やかな赤を維持
    if (displayedTotal > currentTotal) {
        const FillSplit dr = splitTotalHp(displayedTotal, barSegs, CHealth::kSegmentSize);
        drawList.addBarFillSegmented(fillMin, fillSize, kHpDamageColor, barSegs,
                                       dr.fullSegs, dr.tailFillRatio, /*flat=*/true);
    }

    // 緑 fill: 通常通り立体感あり
    const FillSplit gr = splitTotalHp(currentTotal, barSegs, CHealth::kSegmentSize);
    drawList.addBarFillSegmented(fillMin, fillSize, greenColor, barSegs,
                                   gr.fullSegs, gr.tailFillRatio);

    // 現在セグメントだけフラッシュ色で重ね描き (立体感あり)
    if (drawFlashOverlay && currentSegmentIdx >= 1 && currentSegmentIdx <= barSegs && currentHp > 0) {
        const float segFillWidth =
            (totalWidth - kBarInnerPad * 2.f) / static_cast<float>(barSegs);
        const float segFillStartX =
            fillMin.x + segFillWidth * static_cast<float>(currentSegmentIdx - 1);

        const glm::vec2 segFillMin{segFillStartX, fillMin.y};
        const glm::vec2 segFillSize{segFillWidth, fillSize.y};

        const float tailRatio =
            static_cast<float>(currentHp) / static_cast<float>(CHealth::kSegmentSize);
        drawList.addBarFillSegmented(segFillMin, segFillSize, flashColor,
                                       /*segCount=*/1, /*fullSegs=*/0, tailRatio);
    }

    const float midY = originY + kBarHeight * 0.5f;
    for (int i = 1; i < barSegs; ++i) {
        const float x = originX + kBarSegmentWidth * static_cast<float>(i);
        drawList.addRivet({x, midY}, kBarMidRivetRadius, kMidRivetColor);
    }
    drawList.addRivet({originX, midY}, kBarEndCapRadius, kEndCapColor);
    drawList.addRivet({originX + totalWidth, midY}, kBarEndCapRadius, kEndCapColor);
}

float flashCurve(float flash01) {
    return std::pow(std::max(0.f, flash01), 0.6f);
}

}  // namespace

float gripGaugeRadius() {
    return kGripOuterRadius;
}

void drawHealthBar(HudDrawList& drawList, const CHealth& hp, float originX, float originY) {
    // 区画数: HP 0 区画は消える、 死亡時は最低 1 セグメント残す
    const int barSegs = std::max(1, hp.segmentCount);

    const float currentTotal = static_cast<float>(hp.totalHp());
    const float displayedTotal = (hp.displayedTotalHp >= 0.f)
                                   ? hp.displayedTotalHp : currentTotal;

    const float flash01 = (hp.healFlashTimer > 0.f)
                          ? (hp.healFlashTimer / CHealth::kHealFlashDuration)
                          : 0.f;
    const float intensity = flashCurve(flash01);

    const glm::vec4 flashColor = glm::mix(kHpFillColor, kHpFillFlashPeak, intensity);
    const bool drawFlash = (intensity > 0.001f);

    const int currentSegmentIdx = hp.segmentCount;

    drawHpBar(drawList, originX, originY, barSegs, currentSegmentIdx, hp.currentHp,
               currentTotal, displayedTotal,
               kHpFillColor, flashColor, drawFlash);
}

void drawShieldGauge(HudDrawList& drawList, const CShield& shield, float originX,
                      float originY) {
    if (shield.type == ShieldType::None) return;

    const int maxDur = CShield::maxDurability(shield.type);
    if (maxDur <= 0) return;

    const int segCount = (maxDur + kShieldPointsPerGroup - 1) / kShieldPointsPerGroup;

    const int durRemain = shield.durability;
    const int fullSegs = durRemain / kShieldPointsPerGroup;
    const int tailRemain = durRemain - fullSegs * kShieldPointsPerGroup;
    const float tailFillRatio = static_cast<float>(tailRemain) / kShieldPointsPerGroup;

    const glm::vec4& fillColor = shield.guarding ? kShieldGuardingColor : kShieldFillColor;

    drawMechanicalBar(drawList, originX, originY, segCount, fullSegs,
                       tailFillRatio, fillColor);
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
            const bool consumedThis = (i == 0) ? (consumed >= kGripTotalTicks)
                                                : (consumed >= i);
            if (consumedThis) continue;

            const float centerAng = static_cast<float>(i) * segRad;
            float angStart = centerAng - halfSeg + gapRad * 0.5f;
            float angEnd   = centerAng + halfSeg - gapRad * 0.5f;

            if (angStart < 0.f) angStart += 2.f * kPI;
            if (angEnd < 0.f) angEnd += 2.f * kPI;

            const bool isSection = (i % 8) == 0;
            const glm::vec4& tickColor = isSection ? kGripTickSection : kGripTickRegular;

            drawList.addCircleSegment(center, kGripTickOuter, kGripTickInner,
                                       angStart, angEnd, tickColor);
        }
    }

    drawList.addRing(center, kGripOuterRadius, kGripOuterRadius - kGripFrameThick,
                      kGripFrameColor);

    const glm::vec4 orbColor = grip.active() ? grip::def(grip.type).uiOrbColor : kGripOrbNone;
    drawList.addCircle(center, kGripOrbRadius, orbColor, /*gloss=*/true,
                        /*glossStrength=*/0.6f);
}

}  // namespace hud_system

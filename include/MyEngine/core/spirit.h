#pragma once
// =============================================================================
// core/spirit.h — Spirit (アイテム + プレイヤー所持カウンタ)
// =============================================================================
// CSpirit: プレイヤーが拾った spirit の累積個数
// CSpiritPickup: フィールドに浮いてる spirit アイテム本体に付ける
// CFloatingSpirit: spirit アイテムに付ける浮遊挙動 (Rising → Soaring → destruct)
// =============================================================================

#include <glm/glm.hpp>

struct CSpirit {
    int amount = 0;
};

struct CSpiritPickup {};

struct SpiritItemTag {};

struct CFloatingSpirit {
    enum class Phase {
        Rising,
        Soaring,
    };

    Phase phase = Phase::Rising;
    float elapsed = 0.f;

    static constexpr float kSoarStartTime = 8.0f;
    static constexpr float kRiseSpeed     = 0.5f;
    static constexpr float kSoarSpeed     = 20.0f;
    static constexpr float kDestructY     = 10.0f;
};

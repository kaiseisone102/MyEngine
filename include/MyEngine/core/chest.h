#pragma once
// =============================================================================
// core/chest.h — + PotionS
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

#include "core/key.h"

enum class ChestRewardType {
    Coin,
    CoinBag,
    Diamond,
    Armor,
    GoldKey,
    SilverKey,
    FireGrip,
    LightGrip,
    PotionS,  // ← 追加: Small Potion
};

struct ChestReward {
    ChestRewardType type = ChestRewardType::Coin;
    int count = 1;
};

struct CChest {
    enum class State { Closed, Opening, Open };

    static constexpr float kFadeStartTime = 1.0f;
    static constexpr float kFadeDuration  = 2.0f;
    static constexpr float kDestructTime  = kFadeStartTime + kFadeDuration;

    State state = State::Closed;
    float duration = 0.6f;
    float progress = 0.f;
    float openElapsed = 0.f;
    float interactRange = 2.5f;

    bool requiresKey = false;
    KeyType requiredKey = KeyType::Silver;

    std::vector<ChestReward> contents;

    bool isOpenable() const { return state == State::Closed; }
};

struct ChestTag {};

struct CPickupCooldown {
    float remaining = 1.0f;
};

// =============================================================================
// core/chest_preset.cpp
// =============================================================================
#include "core/chest_preset.h"

namespace {

struct PresetEntry {
    const char* name;
    std::vector<ChestReward> contents;
};

const PresetEntry& presetEntry(ChestPresetId id) {
    static const PresetEntry kCoins5 = {
        "Coins5",
        {
            {ChestRewardType::Coin, 5},
        },
    };
    static const PresetEntry kDiamondAndCoin = {
        "DiamondAndCoin",
        {
            {ChestRewardType::Diamond, 1},
            {ChestRewardType::Coin,    1},
        },
    };
    static const PresetEntry kArmor1 = {
        "Armor1",
        {
            {ChestRewardType::Armor, 1},
        },
    };
    static const PresetEntry kFireGrip1 = {
        "FireGrip1",
        {
            {ChestRewardType::FireGrip, 1},
        },
    };
    static const PresetEntry kPotionS1 = {
        "PotionS1",
        {
            {ChestRewardType::PotionS, 1},
        },
    };
    static const PresetEntry kEmpty = {"empty", {}};

    switch (id) {
        case ChestPresetId::Coins5:         return kCoins5;
        case ChestPresetId::DiamondAndCoin: return kDiamondAndCoin;
        case ChestPresetId::Armor1:         return kArmor1;
        case ChestPresetId::FireGrip1:      return kFireGrip1;
        case ChestPresetId::PotionS1:       return kPotionS1;
    }
    return kEmpty;
}

}  // namespace

namespace chest_preset {

const std::vector<ChestReward>& contents(ChestPresetId id) {
    return presetEntry(id).contents;
}

const char* name(ChestPresetId id) {
    return presetEntry(id).name;
}

}  // namespace chest_preset

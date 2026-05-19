#pragma once
// =============================================================================
// core/chest_preset.h — + PotionS1
// =============================================================================

#include <vector>

#include "core/chest.h"

enum class ChestPresetId {
    Coins5,
    DiamondAndCoin,
    Armor1,
    FireGrip1,
    PotionS1,        // ← 追加: Small Potion x 1
};

namespace chest_preset {

const std::vector<ChestReward>& contents(ChestPresetId id);

const char* name(ChestPresetId id);

}  // namespace chest_preset

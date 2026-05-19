#pragma once
// =============================================================================
// item_pickup_system.h — + SoundManager 注入 (pickup SE 用)
// =============================================================================

#include <flecs.h>

#include <vector>

class AssetRegistry;
class SoundManager;

class ItemPickupSystem {
   public:
    void update(std::vector<flecs::entity>& shieldItems,
                std::vector<flecs::entity>& armorItems,
                std::vector<flecs::entity>& gripItems,
                std::vector<flecs::entity>& keyItems,
                std::vector<flecs::entity>& moneyItems,
                std::vector<flecs::entity>& potionItems,
                std::vector<flecs::entity>& spiritItems,
                flecs::entity player, AssetRegistry& assets,
                SoundManager& sound,
                float pickupMargin = 0.5f) const;
};

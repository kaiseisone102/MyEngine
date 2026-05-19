#pragma once
// =============================================================================
// systems/chest_system.h — + SoundManager 注入 (chest 開ける瞬間)
// =============================================================================
// tryOpenNearestChest が Opened を返したときに sound.playChestOpen() を発火。
// update は SE 無関係。
// =============================================================================

#include <flecs.h>

struct WorldData;
class SoundManager;

class ChestSystem {
   public:
    enum class OpenResult {
        NoChest,
        Opened,
        NeedsKey,
    };

    void update(WorldData& wd, float dt) const;

    // 開ける瞬間に sound.playChestOpen() を発火 (Opened 時のみ)。
    OpenResult tryOpenNearestChest(WorldData& wd, SoundManager& sound) const;

    flecs::entity findNearestOpenableChest(WorldData& wd) const;
};

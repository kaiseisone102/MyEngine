#pragma once
// =============================================================================
// spirit_system.h — スピリット浮遊 + ホーミング更新
// =============================================================================

struct WorldData;

class SpiritSystem {
   public:
    void update(WorldData& wd, float dt);
};

#pragma once
// =============================================================================
// systems/gate_system.h — + SoundManager 注入 (gate 開閉 SE)
// =============================================================================
// 開く 2 タイミングで SE 鳴らす:
//   1. tryOpenNearestGate Opened 時 = 開き始め (sound 引数で受ける)
//   2. update 内 Opening → Open 遷移時 = 開き切り (sound 引数で受ける)
// =============================================================================

#include <flecs.h>

struct WorldData;
class SoundManager;

class GateSystem {
   public:
    enum class OpenResult {
        NoGate,
        Opened,
        NeedsKey,
    };

    // 毎フレーム呼ぶ。 Opening → Open 遷移時に sound.playGateOpen() を発火。
    void update(WorldData& wd, float dt, SoundManager& sound) const;

    // 最寄り Closed gate を開く試み。 成功時に sound.playGateOpen() を発火。
    OpenResult tryOpenNearestGate(WorldData& wd, SoundManager& sound) const;

    flecs::entity findNearestOpenableGate(WorldData& wd) const;
};

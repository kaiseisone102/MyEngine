#pragma once
// =============================================================================
// combat_system.h — + SoundManager 注入 (slash SE 用)
// =============================================================================
// 攻撃成立時に slash.mp3 を再生する。 既に攻撃中で無視される場合は無音。
// =============================================================================

#include <flecs.h>

#include "core/attack_def.h"
#include "core/components.h"

struct WorldData;
class SoundManager;

class CombatSystem {
   public:
    // 攻撃要求 (現在 idle なら開始、 そうでなければ無視)。
    // 開始したときだけ sound.playSlash() を発火。
    void requestAttack(flecs::entity attacker, SoundManager& sound) const;
    void requestStrongAttack(flecs::entity attacker, SoundManager& sound) const;

    void update(WorldData& data, flecs::entity attacker, float dt) const;
    void cancelAerialOnLanding(flecs::entity attacker, WorldData& data) const;
    bool isInputLocked(flecs::entity attacker) const;
    AttackPhase getPhase(flecs::entity attacker) const;
    glm::vec3 getCurrentSweepWorldDir(flecs::entity attacker) const;

   private:
    void startAttack(flecs::entity attacker, const AttackDef& def, bool isAerial) const;
    void performSweepHit(WorldData& data, flecs::entity attacker, CAttack& atk,
                          const glm::vec3& prevWorldDir,
                          const glm::vec3& currWorldDir) const;
    void performShockwaveHit(WorldData& data, flecs::entity attacker, CAttack& atk) const;
    void endAttack(flecs::entity attacker) const;
};

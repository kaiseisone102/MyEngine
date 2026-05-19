#pragma once
// =============================================================================
// systems/simulation_system.h
// =============================================================================
// 簡素化: プレイヤー攻撃の hit 判定は CombatSystem に移譲したため、
//   AttackPhaseWindow / pointAabbDistanceSq 等は削除した。
//
// 敵攻撃 hitbox 計算は core/enemy_hitbox_util.h に切り出した
// (judge と debug 描画で共有するため)。
// =============================================================================
#include "core/game_state.h"

struct ActionState;

class SimulationSystem {
   public:
    void updateEnemy(GameState& s, float dt, float gravity);

    // updatePlayer(s, input, dt, gravity, jumpSpeed)
    // input: ActionState (moveX/moveZ/sprint 等を読む)
    void updatePlayer(GameState& s, const ActionState& input, float dt, float gravity,
                      float jumpSpeed);

   private:
    void applyDamageToPlayer(GameState& s, int amount);
    void doRespawn(GameState& s);

    static bool isGroundEnemy(flecs::entity e);
};

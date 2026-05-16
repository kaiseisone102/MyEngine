#pragma once
// =============================================================================
// systems/simulation_system.h
// 役割: 敵・プレイヤーの 1 フレーム分の状態更新を担当する
//       game_loop_orchestrator から戦闘・物理・AI の実装を分離した層
// =============================================================================
#include "core/game_state.h"

class SimulationSystem {
public:
    // 敵シミュレーション（スポーン -> AI -> 物理 -> 衝突 -> ダメージ）
    void updateEnemy(GameState& s, float dt, float gravity);

    // プレイヤーシミュレーション（HP・戦闘・移動・物理）
    void updatePlayer(GameState& s, const bool* keys, float dt, float gravity, float jumpSpeed);

private:
    // ── 戦闘ヘルパー ────────────────────────────────────────────────
    // ダメージ適用（盾吸収 -> HP ダメージ）
    void applyDamageToPlayer(GameState& s, int amount);

    // プレイヤーデス時のリスポーン（位置・速度・HP をリセット）
    void doRespawn(GameState& s);

    // ── 判定ヘルパー ────────────────────────────────────────────────
    // 重力・スライド移動・パンチを持つ「地面を歩く系」敵かどうかを返す
    static bool isGroundEnemy(flecs::entity e);

    // 3 フェーズ（開始 / 中間 / 終端）のどこにいるかを返す
    struct AttackPhaseWindow {
        int   index      = -1;
        float phaseT     = 0.f;
        float progress   = 0.f;
        float windowHalf = 0.08f;
    };
    static AttackPhaseWindow getActiveAttackPhase(const CAttack& atk);

    // 点 p と AABB の最短距離²
    static float pointAabbDistanceSq(const glm::vec3& p, const AABB& b);

    // 地面敵パンチヒットボックス
    struct GroundEnemyPunchHitbox {
        glm::vec3 center;
        glm::vec3 half;
    };
    static GroundEnemyPunchHitbox makeGroundEnemyPunchHitbox(const CTransform& et,
                                                              const CEnemyAI& ai,
                                                              bool isSkeleton);
};

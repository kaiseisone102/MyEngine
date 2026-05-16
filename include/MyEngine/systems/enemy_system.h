#pragma once
// =============================================================================
// enemy_system.h — 敵 AI システム
// =============================================================================
// 責務:
//   - 敵エンティティの状態（Chase / Attack / Cooldown）を毎フレーム更新する
//   - プレイヤーに一直線に追尾し、近づいたらパンチを放つ
//   - 障害物は無視（ナビゲーションなし）
// =============================================================================

#include <flecs.h>

#include <vector>

class EnemySystem {
   public:
    // enemies    : 管理する敵エンティティの一覧
    // player     : パンチの目標となるプレイヤーエンティティ
    // dt         : デルタタイム（秒）
    void update(const std::vector<flecs::entity>& enemies, flecs::entity player, float dt) const;
};

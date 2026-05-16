#pragma once
// =============================================================================
// spawn_system.h — スポーントリガーシステム
// =============================================================================
// 責務:
//   - 毎フレーム、プレイヤーの位置と各 SpawnTrigger のゾーンを照合する
//   - プレイヤーがゾーン内に入ったら、指定の敵エンティティを生成してリストに追加する
//   - 発動済みトリガーは再発動しない（ワンショット）
// =============================================================================

#include <flecs.h>

#include <vector>

#include "core/spawn_trigger.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// スケルトン生成で使う定数（初期配置・トリガーで共通）
namespace EnemySpawnDefaults {
constexpr float kSkeletonAttackStartupLag = 0.20f;
}

class SpawnSystem {
   public:
    // スケルトン 1 体を生成する唯一の入口（初期配置・SpawnTrigger 共通）
    // spawnStunSeconds: トリガー直後の硬直（初期配置は 0）
    // outEnemies: 非 null ならリストに追加
    static flecs::entity createSkeleton(flecs::world& world, const char* entityName,
                                        const glm::vec3& pos,
                                        std::vector<flecs::entity>* outEnemies,
                                        float spawnStunSeconds = 0.f);
    // triggers : SpawnTrigger の一覧（fired フラグを書き換える）
    // player   : プレイヤーエンティティ（位置取得に使用）
    // enemies  : 新しく生成した敵をここに追加する
    // world    : flecs::entity を生成するために必要
    void update(std::vector<SpawnTrigger>& triggers, flecs::entity player,
                std::vector<flecs::entity>& enemies, flecs::world& world) const;
};

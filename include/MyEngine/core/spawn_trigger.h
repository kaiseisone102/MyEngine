#pragma once
// =============================================================================
// spawn_trigger.h — エリア進入時の敵スポーン定義
// =============================================================================
// プレイヤーが center 中心 radius 半径の範囲に入ったら、 spawns に列挙された
// 敵を一括でスポーンする。 stage_registry.cpp 等で使用。
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

enum class EnemyType {
    Skeleton,
    Ghost,
};

struct SpawnEntry {
    EnemyType type;
    glm::vec3 position;
};

struct SpawnTrigger {
    glm::vec3 center{0.f, 0.f, 0.f};
    float radius{0.f};
    std::vector<SpawnEntry> spawns;
    bool triggered{false};  // 1 度発動後は再発動しないためのフラグ
};

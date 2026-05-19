#pragma once
// =============================================================================
// stage_def.h — ステージ定義 + 一覧 (StageRegistry)
// =============================================================================
// 各ステージの:
//   - 表示名 (Settings/Pause menu の "Warp to" で表示)
//   - プレイヤー初期位置
//   - ジオメトリ + 敵 + アイテム + WarpPad の配置 (関数として渡す)
// を 1 箇所にまとめる。
//
// 新ステージ追加手順:
//   1. stage_id.h の enum に StageId::Stage1_3 等を追加
//   2. stage_registry.cpp の kStageDefs に
//      StageDef{StageId::Stage1_3, "Stage 1-3", spawn, [](WorldData&){ ... }}
//      を追加
// 以上で:
//   - Terminal の WarpPad リストに自動的に追加される
//   - GameplayLayer の ESC > Warp サブメニューにも自動的に表示される
// =============================================================================

#include <functional>
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "world/stage_id.h"

struct WorldData;

struct StageDef {
    StageId id;
    std::string name;             // "Terminal", "Stage 1-1"
    glm::vec3 playerSpawnPos;     // プレイヤー初期位置 (足元基準)
    std::function<void(WorldData&)> buildContent;  // ジオメトリ・敵・アイテム配置
};

namespace stage_registry {

// 全ステージの定義 (Terminal を含む)
const std::vector<StageDef>& all();

// 指定 StageId のステージ定義を取得 (見つからない場合は assert + Terminal を返す)
const StageDef& get(StageId id);

// Terminal を除く「実ステージ」 のリスト (ターミナルの WarpPad 配置用)
std::vector<const StageDef*> realStages();

}  // namespace stage_registry

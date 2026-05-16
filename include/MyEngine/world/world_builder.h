// include/world/world_builder.h
#pragma once
// =============================================================================
// world/world_builder.h
// 役割: マップ・プレイヤー・敵・アイテムの初期配置を担当する
//       EngineApp::initGame() から Vulkan / SDL / Audio
//       の初期化を除いた純粋なゲームデータ構築層
// =============================================================================
#include "core/game_state.h"

class WorldBuilder {
   public:
    // WorldData を受け取り、全サブ構築を順番に呼び出す
    void build(WorldData& data);

   private:
    // プレイヤーエンティティの生成
    void buildPlayer(WorldData& data);

    // プラットフォーム / 壁 エンティティの生成
    void buildPlatforms(WorldData& data);

    // 初期配置の敵（スタート時から存在するもの）
    void buildInitialEnemies(WorldData& data);

    // スポーントリガーの登録
    void buildSpawnTriggers(WorldData& data);

    // アイテム（盾・アーマー）の配置
    void buildItems(WorldData& data);

    void buildTestEquipment(WorldData& data);

    void setupPlayerEquipment(WorldData& data, const Model* knightModel);
};

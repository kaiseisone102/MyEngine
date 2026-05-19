// include/world/world_builder.h
#pragma once
// =============================================================================
// world/world_builder.h — ステージ駆動版 + Player 持続オプション
// =============================================================================
// 役割: 指定された StageId に応じてマップ・プレイヤー・敵・アイテムを
//       構築する。 ステージ固有のジオメトリ・敵・アイテムは
//       StageRegistry から取得した buildContent コールバックで配置される。
//
//       Player と装備はステージ非依存 (共通) で WorldBuilder が直接組み立てる。
//       Terminal でもステージ 1-1 でも同じ player モデル + 装備でスタート。
//
// keepPlayer モード (新):
//       ワープ時のように「player の状態 (HP/盾/その他コンポーネント) を
//       引き継ぎたい」 場合に使う。 reset(keepPlayer=true) で player を
//       destruct せず、 build(keepPlayer=true) で player の再生成を skip。
//       ただし位置だけは新ステージの spawnPos に更新する。
//
//       これにより:
//         - player が持つ全コンポーネントが自動的に持続 (HP, 盾,
//           CEquipment, CAttack 状態, アニメ slot 等)
//         - 将来追加するコンポーネント (経験値、 スキル、 インベントリ等)
//           も自動で持続 — WorldBuilder の変更不要
//
// 用途:
//   - 新規ゲーム開始 / GameOver → リスポーン: keepPlayer=false (デフォルト)
//   - ワープパッド / メニュー Warp        : keepPlayer=true
// =============================================================================
#include "core/game_state.h"
#include "world/stage_id.h"

class WorldBuilder {
   public:
    // 指定ステージで WorldData を構築する。
    //   keepPlayer=false (default): Player を新規生成 (CHealth 等はデフォルト値)
    //   keepPlayer=true           : Player は既存のまま、 位置だけ spawnPos に更新
    void build(WorldData& data, StageId stageId, bool keepPlayer = false);

    // 既存 entity を破棄して空の WorldData に戻す (build と対)。
    //   keepPlayer=false (default): Player も destruct
    //   keepPlayer=true           : Player を destruct しない (敵・地形等のみ破棄)
    void reset(WorldData& data, bool keepPlayer = false);

   private:
    void buildPlayer(WorldData& data, const glm::vec3& spawnPos);
    void setupPlayerEquipment(WorldData& data, const Model* knightModel);
};

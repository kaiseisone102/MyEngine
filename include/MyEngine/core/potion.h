#pragma once
// =============================================================================
// core/potion.h — HP 回復ポーション
// =============================================================================
// PotionType: S/M/L 拡張を見越した enum (今回は S のみ実装)。
// money/key と同じパターン。
//
// CPotionPickup: 地面に置かれた potion アイテムに付ける。
// PotionItemTag: ECS クエリ用タグ。
//
// 効果:
//   S: 現在区画 HP を +1 (kSegmentSize 上限)
//      満タンなら拾わない (item_pickup_system 側で判定)
// =============================================================================

enum class PotionType {
    Small,   // HP +1
    // Medium, Large は将来追加
};

struct CPotionPickup {
    PotionType type = PotionType::Small;
};

struct PotionItemTag {};

namespace potion {

// PotionType → モデル名 ("potion_s" 等)
const char* modelName(PotionType type);

// PotionType → ログ用名前 ("Small Potion")
const char* typeName(PotionType type);

// PotionType → HP 回復量
int healAmount(PotionType type);

}  // namespace potion

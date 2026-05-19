#pragma once
// =============================================================================
// equipment_util.h — Phase 5-D
// =============================================================================
// 装備変更を統一的に扱うユーティリティ。
//
// 設計目標:
//   - ShieldType と Model* の対応を 1 箇所に集約
//   - 盾の取得・破壊・初期化・リスポーン などすべての場面から同じヘルパーを使う
//   - 将来 swordType / armorType を追加する場合も同じパターンで拡張可能
//
// 使い方:
//   - shieldModelForType(assets, ShieldType::Normal) → const Model* (shield_iron)
//   - applyShieldChange(player, assets, ShieldType::Silver) → CShield + CEquipment 更新
// =============================================================================

#include <flecs.h>

#include "core/components.h"

class AssetRegistry;
class Model;

namespace equipment {

// ShieldType に対応する盾モデルを返す。 None または不明なら nullptr。
const Model* shieldModelForType(AssetRegistry& assets, ShieldType t);

// Player の CShield と CEquipment を newType に合わせて一括更新。
//   - CShield.type / durability を更新
//   - CEquipment.leftHandModel / leftHandVisible を更新
//   - 装備品が見つからない場合は leftHandVisible=false
//
// 盾拾い・破壊・初期化・リスポーン から共通的に呼べる。
void applyShieldChange(flecs::entity player, AssetRegistry& assets, ShieldType newType);

}  // namespace equipment

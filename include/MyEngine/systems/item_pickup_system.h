// C:\MyEngine\include\MyEngine\systems\item_pickup_system.h

#pragma once
// =============================================================================
// item_pickup_system.h — アイテム拾得システム
// =============================================================================
// プレイヤーが一定範囲内に入ったアイテムを自動的に拾得する。
// 現在は盾アイテム（ShieldItemTag を持つエンティティ）のみ対応。
// 将来の拡張: 武器・回復アイテムなども同じ仕組みで扱える。
// =============================================================================

#include <flecs.h>

#include <vector>

class ItemPickupSystem {
   public:
    // shieldItems / armorItems の中でプレイヤーの pickupRadius (m) 以内にあるものを拾得する。
    //   盾アイテム  : プレイヤーの CShield を更新
    //   アーマー    : プレイヤーの CHealth::addSegment() を呼ぶ（既に上限なら拾わない）
    void update(std::vector<flecs::entity>& shieldItems, std::vector<flecs::entity>& armorItems,
                flecs::entity player, float pickupRadius = 1.2f) const;
};

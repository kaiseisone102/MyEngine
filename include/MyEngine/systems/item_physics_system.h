#pragma once
// =============================================================================
// systems/item_physics_system.h — アイテム用簡易物理
// =============================================================================
// 宝箱から飛び出した直後のアイテムに重力 + 着地ロジックを適用する。
// CVelocity を持つアイテム (Money/Armor/Key 等) を対象とし、 Y 速度がゼロに
// なって地面に乗ったら CVelocity を消す (= 通常の静止アイテムに戻る)。
//
// プレイヤー/敵物理とは独立。 シンプル化のためアイテム間衝突は無視。
// =============================================================================

#include <flecs.h>

#include <vector>

struct WorldData;

class ItemPhysicsSystem {
   public:
    void update(WorldData& wd, float dt, float gravity) const;
};

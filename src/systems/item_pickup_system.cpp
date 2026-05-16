// C:\MyEngine\src\systems\item_pickup_system.cpp

#define NOMINMAX
// =============================================================================
// item_pickup_system.cpp — アイテム拾得システム実装
// =============================================================================
// 判定ロジック:
//   1. プレイヤーの XZ 座標とアイテムの XZ 座標の距離を計算
//   2. pickupRadius 以内なら拾得 → CShield を更新してアイテムを削除
//   3. すでに盾を持っていても拾える（より良い盾に付け替え可能）
// =============================================================================

#include "systems/item_pickup_system.h"

#include "core/components.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <algorithm>
#include <glm/glm.hpp>

void ItemPickupSystem::update(std::vector<flecs::entity>& shieldItems,
                              std::vector<flecs::entity>& armorItems, flecs::entity player,
                              float pickupRadius) const {
    const glm::vec3& pPos = player.get<CTransform>().pos;

    // ── 盾アイテム ────────────────────────────────────────────────────────
    shieldItems.erase(std::remove_if(shieldItems.begin(), shieldItems.end(),
                                     [&](flecs::entity item) {
                                         const CTransform& it = item.get<CTransform>();
                                         const glm::vec2 diff{pPos.x - it.pos.x, pPos.z - it.pos.z};
                                         if (glm::length(diff) > pickupRadius) return false;

                                         const CPickup& pickup = item.get<CPickup>();
                                         CShield& sh = player.ensure<CShield>();
                                         sh.type = pickup.shieldType;
                                         sh.durability = CShield::maxDurability(sh.type);

                                         item.destruct();
                                         return true;
                                     }),
                      shieldItems.end());

    // ── アーマーアイテム（区画 +1）──────────────────────────────────────
    // 区画が上限に達している場合は拾わない（アイテムはマップに残る）。
    armorItems.erase(std::remove_if(armorItems.begin(), armorItems.end(),
                                    [&](flecs::entity item) {
                                        const CTransform& it = item.get<CTransform>();
                                        const glm::vec2 diff{pPos.x - it.pos.x, pPos.z - it.pos.z};
                                        if (glm::length(diff) > pickupRadius) return false;

                                        CHealth& hp = player.ensure<CHealth>();
                                        if (!hp.addSegment())
                                            return false;  // 上限に達している → 拾わない

                                        item.destruct();
                                        return true;
                                    }),
                     armorItems.end());
}

#define NOMINMAX
// =============================================================================
// item_pickup_system.cpp — + armor / potion / spirit SE (ターン3 仮代用)
// =============================================================================
#include "systems/item_pickup_system.h"
#include <iostream>
#include "core/chest.h"
#include "core/components.h"
#include "core/cylinder.h"
#include "core/equipment_util.h"
#include "core/grip.h"
#include "core/key.h"
#include "core/money.h"
#include "core/potion.h"
#include "core/spirit.h"
#include "systems/sound_manager.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <algorithm>
#include <glm/glm.hpp>
namespace {
bool itemInRange(const Cylinder& playerCyl, const glm::vec3& itemPos, float margin) {
    const float dx = playerCyl.baseCenter.x - itemPos.x;
    const float dz = playerCyl.baseCenter.z - itemPos.z;
    const float distSq = dx * dx + dz * dz;
    const float effRadius = playerCyl.radius + margin;
    if (distSq > effRadius * effRadius) return false;
    const float yMin = playerCyl.bottomY() - margin;
    const float yMax = playerCyl.topY() + margin;
    if (itemPos.y < yMin || itemPos.y > yMax) return false;
    return true;
}
bool itemPickable(flecs::entity item) {
    if (item.has<CVelocity>()) return false;
    if (item.has<CPickupCooldown>()) return false;
    return true;
}
}  // namespace
void ItemPickupSystem::update(std::vector<flecs::entity>& shieldItems,
                              std::vector<flecs::entity>& armorItems,
                              std::vector<flecs::entity>& gripItems,
                              std::vector<flecs::entity>& keyItems,
                              std::vector<flecs::entity>& moneyItems,
                              std::vector<flecs::entity>& potionItems,
                              std::vector<flecs::entity>& spiritItems,
                              flecs::entity player, AssetRegistry& assets,
                              SoundManager& sound,
                              float pickupMargin) const {
    const CTransform& ptr = player.get<CTransform>();
    const Cylinder playerCyl = Cylinder::fromBottomCenter(ptr.pos, ptr.scale);

    // ─── Shield ──────────────────────────────────────────────
    shieldItems.erase(std::remove_if(shieldItems.begin(), shieldItems.end(),
                                     [&](flecs::entity item) {
                                         if (!itemPickable(item)) return false;
                                         const CTransform& it = item.get<CTransform>();
                                         if (!itemInRange(playerCyl, it.pos, pickupMargin))
                                             return false;
                                         const CPickup& pickup = item.get<CPickup>();
                                         equipment::applyShieldChange(player, assets,
                                                                       pickup.shieldType);
                                         sound.playPickupShield();
                                         item.destruct();
                                         return true;
                                     }),
                      shieldItems.end());

    // ─── Armor (仮代用 coin.wav) ────────────────────────────
    armorItems.erase(std::remove_if(armorItems.begin(), armorItems.end(),
                                    [&](flecs::entity item) {
                                        if (!itemPickable(item)) return false;
                                        const CTransform& it = item.get<CTransform>();
                                        if (!itemInRange(playerCyl, it.pos, pickupMargin))
                                            return false;
                                        CHealth& hp = player.ensure<CHealth>();
                                        if (!hp.addSegment()) return false;
                                        sound.playPickupArmor();
                                        item.destruct();
                                        return true;
                                    }),
                     armorItems.end());

    // ─── Grip ────────────────────────────────────────────────
    gripItems.erase(std::remove_if(gripItems.begin(), gripItems.end(),
                                    [&](flecs::entity item) {
                                        if (!itemPickable(item)) return false;
                                        const CTransform& it = item.get<CTransform>();
                                        if (!itemInRange(playerCyl, it.pos, pickupMargin))
                                            return false;
                                        const CGripPickup& pickup = item.get<CGripPickup>();
                                        grip::applyGripPickup(player, pickup.type);
                                        sound.playPickupGrip();
                                        item.destruct();
                                        return true;
                                    }),
                     gripItems.end());

    // ─── Key ─────────────────────────────────────────────────
    keyItems.erase(std::remove_if(keyItems.begin(), keyItems.end(),
                                    [&](flecs::entity item) {
                                        if (!itemPickable(item)) return false;
                                        const CTransform& it = item.get<CTransform>();
                                        if (!itemInRange(playerCyl, it.pos, pickupMargin))
                                            return false;
                                        const CKeyPickup& pickup = item.get<CKeyPickup>();
                                        CKeyInventory& inv = player.ensure<CKeyInventory>();
                                        inv.give(pickup.type, 1);
                                        sound.playPickupKey();
                                        std::cout << "[Key] picked up "
                                                  << keyTypeName(pickup.type)
                                                  << " key, total="
                                                  << inv.count(pickup.type) << "\n";
                                        item.destruct();
                                        return true;
                                    }),
                     keyItems.end());

    // ─── Money ──────────────────────────────────────────────
    moneyItems.erase(std::remove_if(moneyItems.begin(), moneyItems.end(),
                                    [&](flecs::entity item) {
                                        if (!itemPickable(item)) return false;
                                        const CTransform& it = item.get<CTransform>();
                                        if (!itemInRange(playerCyl, it.pos, pickupMargin))
                                            return false;
                                        const CMoneyPickup& pickup = item.get<CMoneyPickup>();
                                        const int value = moneyValue(pickup.type);
                                        CMoney& money = player.ensure<CMoney>();
                                        money.amount += value;
                                        sound.playPickupMoney(pickup.type);
                                        std::cout << "[Money] picked up "
                                                  << moneyTypeName(pickup.type) << " (+"
                                                  << value << "), total=" << money.amount
                                                  << "\n";
                                        item.destruct();
                                        return true;
                                    }),
                     moneyItems.end());

    // ─── Potion (仮代用 coin.wav) ───────────────────────────
    potionItems.erase(std::remove_if(potionItems.begin(), potionItems.end(),
                                    [&](flecs::entity item) {
                                        if (!itemPickable(item)) return false;
                                        const CTransform& it = item.get<CTransform>();
                                        if (!itemInRange(playerCyl, it.pos, pickupMargin))
                                            return false;
                                        const CPotionPickup& pickup = item.get<CPotionPickup>();
                                        CHealth& hp = player.ensure<CHealth>();
                                        const int amount = potion::healAmount(pickup.type);
                                        const bool healed = hp.healInCurrentSegment(amount);
                                        hp.healFlashTimer = CHealth::kHealFlashDuration;
                                        sound.playPickupPotion();
                                        std::cout << "[Potion] picked up "
                                                  << potion::typeName(pickup.type)
                                                  << (healed ? " (+" : " (no heal, ")
                                                  << (healed ? std::to_string(amount).c_str() : "max HP")
                                                  << (healed ? " HP), HP " : "), HP ")
                                                  << hp.currentHp << "/"
                                                  << CHealth::kSegmentSize << "\n";
                                        item.destruct();
                                        return true;
                                    }),
                     potionItems.end());

    // ─── Spirit (仮代用 coin.wav) ───────────────────────────
    spiritItems.erase(std::remove_if(spiritItems.begin(), spiritItems.end(),
                                    [&](flecs::entity item) {
                                        if (!item.is_alive()) return true;
                                        const CTransform& it = item.get<CTransform>();
                                        if (!itemInRange(playerCyl, it.pos, pickupMargin))
                                            return false;
                                        CSpirit& sp = player.ensure<CSpirit>();
                                        sp.amount += 1;
                                        sound.playPickupSpirit();
                                        std::cout << "[Spirit] picked up (+1), total="
                                                  << sp.amount << "\n";
                                        item.destruct();
                                        return true;
                                    }),
                     spiritItems.end());
}

// src/core/equipment_util.cpp — Phase 5-D
#include "core/equipment_util.h"

#include <iostream>

#include "renderer/asset_registry.h"
#include "renderer/model.h"

namespace equipment {

const Model* shieldModelForType(AssetRegistry& assets, ShieldType t) {
    switch (t) {
        case ShieldType::Iron:
            return assets.getModel("shield_iron");
        case ShieldType::Silver:
            return assets.getModel("shield_silver");
        case ShieldType::Gold:
            return assets.getModel("shield_gold");
        case ShieldType::None:
        default:
            return nullptr;
    }
}

void applyShieldChange(flecs::entity player, AssetRegistry& assets, ShieldType newType) {
    // CShield 更新
    CShield& sh = player.ensure<CShield>();
    sh.type = newType;
    sh.durability = CShield::maxDurability(newType);

    // CEquipment 更新
    if (!player.has<CEquipment>()) {
        std::cerr << "[Equipment] WARNING: applyShieldChange called on entity without CEquipment\n";
        return;
    }
    CEquipment& eq = player.ensure<CEquipment>();

    if (newType == ShieldType::None) {
        eq.leftHandModel = nullptr;
        eq.leftHandVisible = false;
    } else {
        const Model* m = shieldModelForType(assets, newType);
        eq.leftHandModel = m;
        eq.leftHandVisible = (m != nullptr && !m->empty());
    }
}

}  // namespace equipment

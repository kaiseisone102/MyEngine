// src/core/equipment_util.cpp - Phase 5-D + ModelScale Registry support
#include "core/equipment_util.h"

#include <iostream>

#include "renderer/asset_registry.h"
#include "renderer/model.h"
#include "renderer/model_scale_registry.h"

namespace equipment {

namespace {

// Model name corresponding to ShieldType (key in model_scale Registry)
const char* shieldModelName(ShieldType t) {
    switch (t) {
        case ShieldType::Iron:
            return "shield_iron";
        case ShieldType::Silver:
            return "shield_silver";
        case ShieldType::Gold:
            return "shield_gold";
        case ShieldType::None:
        default:
            return nullptr;
    }
}

}  // namespace

const Model* shieldModelForType(AssetRegistry& assets, ShieldType t) {
    const char* name = shieldModelName(t);
    return name ? assets.getModel(name) : nullptr;
}

void applyShieldChange(flecs::entity player, AssetRegistry& assets, ShieldType newType) {
    // Update CShield
    CShield& sh = player.ensure<CShield>();
    sh.type = newType;
    sh.durability = CShield::maxDurability(newType);

    // Update CEquipment
    if (!player.has<CEquipment>()) {
        std::cerr << "[Equipment] WARNING: applyShieldChange called on entity without CEquipment\n";
        return;
    }
    CEquipment& eq = player.ensure<CEquipment>();

    if (newType == ShieldType::None) {
        eq.leftHandModel = nullptr;
        eq.leftHandVisible = false;
        // scale stays as-is (no visible effect since not rendered)
    } else {
        const char* name = shieldModelName(newType);
        const Model* m = assets.getModel(name);
        eq.leftHandModel = m;
        eq.leftHandVisible = (m != nullptr && !m->empty());
        // Get equipped-time scale from model_scale Registry (Context::Equipped).
        // Size difference between a shield on the ground and one equipped is managed by the
        // Registry.
        eq.leftHandScale = model_scale::get(name, model_scale::Context::Equipped);
    }
}

}  // namespace equipment

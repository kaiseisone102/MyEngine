// src/core/equipment_util.cpp — Phase 5-D + ModelScale Registry 対応
#include "core/equipment_util.h"

#include <iostream>

#include "renderer/asset_registry.h"
#include "renderer/model.h"
#include "renderer/model_scale_registry.h"

namespace equipment {

namespace {

// ShieldType に対応するモデル名 (model_scale Registry のキー)
const char* shieldModelName(ShieldType t) {
    switch (t) {
        case ShieldType::Iron:   return "shield_iron";
        case ShieldType::Silver: return "shield_silver";
        case ShieldType::Gold:   return "shield_gold";
        case ShieldType::None:
        default:                 return nullptr;
    }
}

}  // namespace

const Model* shieldModelForType(AssetRegistry& assets, ShieldType t) {
    const char* name = shieldModelName(t);
    return name ? assets.getModel(name) : nullptr;
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
        // scale はそのまま (見えないので影響なし)
    } else {
        const char* name = shieldModelName(newType);
        const Model* m = assets.getModel(name);
        eq.leftHandModel = m;
        eq.leftHandVisible = (m != nullptr && !m->empty());
        // 装備時のスケールを model_scale Registry (Context::Equipped) から取得。
        // 床に落ちている盾と装備中の盾でサイズが違うのは、 Registry がそれを管理。
        eq.leftHandScale = model_scale::get(name, model_scale::Context::Equipped);
    }
}

}  // namespace equipment

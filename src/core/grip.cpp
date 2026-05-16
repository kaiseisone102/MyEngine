// =============================================================================
// core/grip.cpp — Grip システム実装 (炎サイズ 0.8 倍 + UI 色)
// =============================================================================
#include "core/grip.h"

#include <array>

#include "core/components.h"

namespace {

using particle::BlendMode;
using particle::EmitterShape;

void applyPresetToEmitter(CParticleEmitter& em, const GripEmitterPreset& p,
                          const glm::vec4& colorStart, const glm::vec4& colorEnd) {
    em.emitRate = p.emitRate;
    em.lifetimeMin = p.lifetimeMin;
    em.lifetimeMax = p.lifetimeMax;
    em.sizeStartMin = p.sizeStartMin;
    em.sizeStartMax = p.sizeStartMax;
    em.sizeEndMin = p.sizeEndMin;
    em.sizeEndMax = p.sizeEndMax;
    em.gravity = p.gravity;
    em.drag = p.drag;
    em.shape = p.shape;
    em.shapeParams = p.shapeParams;
    em.emitDirectionLocal = p.emitDirectionLocal;
    em.speedMin = p.speedMin;
    em.speedMax = p.speedMax;
    em.velocityRandomCone = p.velocityRandomCone;
    em.colorStart = colorStart;
    em.colorEnd = colorEnd;
    em.blendMode = BlendMode::Additive;
}

// 炎プリセット (装備中)
const std::array<GripDef, 2> kGripDefs = {{
    GripDef{
        "none", "none", 0, glm::vec4{1.f}, glm::vec4{1.f},
        GripEmitterPreset{0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, glm::vec3{0.f}, 0.f,
                          EmitterShape::Point, glm::vec3{0.f}, glm::vec3{0.f, 1.f, 0.f}, 0.f, 0.f,
                          0.f},
        GripEmitterPreset{0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, glm::vec3{0.f}, 0.f,
                          EmitterShape::Point, glm::vec3{0.f}, glm::vec3{0.f, 1.f, 0.f}, 0.f, 0.f,
                          0.f},
        glm::vec4{0.3f, 1.0f, 0.4f, 1.0f}  // 装着なし時 = 緑
    },
    GripDef{
        "fire", "fire_grip", 32, glm::vec4{1.0f, 1.0f, 0.6f, 1.0f},
        glm::vec4{1.0f, 0.3f, 0.0f, 0.0f},
        // 装備中
        GripEmitterPreset{60.f, 0.4f, 0.7f, 0.06f, 0.10f,  // sizeStart
                          0.18f, 0.28f,                    // sizeEnd
                          glm::vec3{0.f, 2.0f, 0.f}, 1.5f, EmitterShape::Line,
                          glm::vec3{0.f, 0.5f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, 1.5f, 2.5f, 0.3f},
        // 地面アイテム (サイズ 0.8 倍適用)
        GripEmitterPreset{25.f, 0.5f, 0.9f, 0.032f, 0.056f,  // sizeStart 0.8x (旧 0.04/0.07)
                          0.096f, 0.144f,                    // sizeEnd 0.8x (旧 0.12/0.18)
                          glm::vec3{0.f, 1.0f, 0.f}, 1.0f, EmitterShape::Sphere,
                          glm::vec3{0.2f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, 0.5f, 1.0f, 0.5f},
        glm::vec4{0.95f, 0.20f, 0.10f, 1.0f}  // Fire = 赤
    },
}};

}  // namespace

namespace grip {

const GripDef& def(GripType t) {
    const auto idx = static_cast<size_t>(t);
    if (idx < kGripDefs.size()) return kGripDefs[idx];
    return kGripDefs[0];
}

void syncEmitter(flecs::entity player) {
    if (!player || !player.is_alive()) return;
    if (!player.has<CParticleEmitter>()) return;
    if (!player.has<CGrip>()) {
        auto& em = player.ensure<CParticleEmitter>();
        em.emitting = false;
        return;
    }

    const CGrip& g = player.get<CGrip>();
    auto& em = player.ensure<CParticleEmitter>();

    if (!g.active()) {
        em.emitting = false;
        return;
    }

    const GripDef& d = def(g.type);
    em.emitting = true;
    applyPresetToEmitter(em, d.equipped, d.colorStart, d.colorEnd);
}

void applyGripPickup(flecs::entity player, GripType t) {
    if (!player || !player.is_alive()) return;
    auto& g = player.ensure<CGrip>();

    if (t == GripType::None) {
        g.type = GripType::None;
        g.durability = 0;
    } else if (g.type == t) {
        g.durability = def(t).maxDurability;
    } else {
        g.type = t;
        g.durability = def(t).maxDurability;
    }

    syncEmitter(player);
}

void consumeOnHit(flecs::entity player) {
    if (!player || !player.is_alive()) return;
    if (!player.has<CGrip>()) return;
    auto& g = player.ensure<CGrip>();
    if (g.durability <= 0) return;

    g.durability--;
    if (g.durability <= 0) {
        syncEmitter(player);
    }
}

void attachItemEmitter(flecs::entity item, GripType t) {
    if (!item || !item.is_alive()) return;
    if (t == GripType::None) return;

    const GripDef& d = def(t);

    CParticleEmitter em;
    em.attachBoneIdx = -1;
    em.localOffset = glm::mat4(1.f);
    em.emitting = true;

    applyPresetToEmitter(em, d.onItem, d.colorStart, d.colorEnd);

    item.set<CParticleEmitter>(em);
}

}  // namespace grip

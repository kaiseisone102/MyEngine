#pragma once
// =============================================================================
// core/grip.h — Grip システム (剣の属性付与アイテム)
// =============================================================================
// GripDef::uiOrbColor で「Grip ゲージの中の丸の色」 を種類ごとに指定。
// Fire=赤、 Light=黄白、 (将来) Ice=青 等でタイプアイデンティティを表現。
// =============================================================================

#include <cstdint>
#include <flecs.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/particle.h"

enum class GripType : uint8_t {
    None = 0,
    Fire,
    Light,
};

struct GripEmitterPreset {
    float emitRate;
    float lifetimeMin;
    float lifetimeMax;
    float sizeStartMin;
    float sizeStartMax;
    float sizeEndMin;
    float sizeEndMax;
    glm::vec3 gravity;
    float drag;
    particle::EmitterShape shape;
    glm::vec3 shapeParams;
    glm::vec3 emitDirectionLocal;
    float speedMin;
    float speedMax;
    float velocityRandomCone;
};

struct GripDef {
    const char* name;
    const char* modelName;
    int maxDurability;
    glm::vec4 colorStart;
    glm::vec4 colorEnd;
    GripEmitterPreset equipped;
    GripEmitterPreset onItem;
    glm::vec4 uiOrbColor;
};

namespace grip {

const GripDef& def(GripType t);
void applyGripPickup(flecs::entity player, GripType t);
void consumeOnHit(flecs::entity player);
void syncEmitter(flecs::entity player);
void attachItemEmitter(flecs::entity item, GripType t);

}  // namespace grip

struct CGrip {
    GripType type = GripType::None;
    int durability = 0;

    bool active() const { return type != GripType::None && durability > 0; }
};

struct CGripPickup {
    GripType type = GripType::None;
};

struct GripItemTag {};

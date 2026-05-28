// =============================================================================
// core/grave_fx.cpp - Grave-specific particle control (blue firefly)
// =============================================================================
// The grave_spirit model has its origin at the top of the grave, so
// to put the emitter at the foot, set localOffset.y to negative. The top is at y=0.
//
// shape:
//   shape: Box (wide emission area around grave)
//   shapeParams = half extents = (1.2, 1.0, 1.2)
//      -> emits within XZ +/-1.2m, Y +/-1.0m (spreads around the grave body too)
//   localOffset: y = -0.5 (places emission center at grave middle)
//
// velocity:
//   emitDirectionLocal: upward
//   speed: 0.4 - 0.8 m/s (slow rising)
//   cone: 0.9 (wide scatter, swaying feel)
//
// Lifetime and size:
//   lifetime: 2.0 - 3.5 sec (long; flows upward while rising slowly)
//   emitRate: 1.5/s (sparse; maintains 3-5 at a time)
//   size: 0.08-0.12 -> 0.16-0.20 (fades while expanding)
//
// gravity:
//   gravity: 0.5 (slight upward buoyancy, gentle rising feel)
//   drag: 0.5
// =============================================================================
#include "core/grave_fx.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/components.h"
#include "core/grave.h"
#include "core/particle.h"

namespace {

void applyBluefireflyPreset(CParticleEmitter& em) {
    em.attachBoneIdx = -1;

    // origin is at grave top, so -0.5 places emission area center at grave middle
    em.localOffset = glm::translate(glm::mat4(1.f), glm::vec3{0.f, -0.5f, 0.f});

    // Box: covers the grave body and also spreads outward
    em.shape = particle::EmitterShape::Box;
    em.shapeParams = glm::vec3{1.2f, 1.0f, 1.2f};  // half extents

    em.emitDirectionLocal = glm::vec3{0.f, 1.f, 0.f};
    em.speedMin = 0.4f;
    em.speedMax = 0.8f;
    em.velocityRandomCone = 0.9f;  // wide scatter

    em.emitRate = 1.5f;
    em.lifetimeMin = 2.0f;
    em.lifetimeMax = 3.5f;

    em.sizeStartMin = 0.08f;
    em.sizeStartMax = 0.12f;
    em.sizeEndMin = 0.16f;
    em.sizeEndMax = 0.20f;

    em.colorStart = glm::vec4{0.4f, 0.8f, 1.0f, 0.8f};
    em.colorEnd = glm::vec4{0.1f, 0.3f, 1.0f, 0.0f};

    em.gravity = glm::vec3{0.f, 0.5f, 0.f};  // slight upward buoyancy
    em.drag = 0.5f;

    em.blendMode = particle::BlendMode::Additive;
}

}  // namespace

namespace grave_fx {

void attachEmitter(flecs::entity grave) {
    if (!grave || !grave.is_alive()) return;

    CParticleEmitter em;
    applyBluefireflyPreset(em);
    em.emitting = true;

    grave.set<CParticleEmitter>(em);
}

void syncEmitter(flecs::entity grave) {
    if (!grave || !grave.is_alive()) return;
    if (!grave.has<CParticleEmitter>()) return;
    if (!grave.has<CGrave>()) return;

    const CGrave& g = grave.get<CGrave>();
    auto& em = grave.ensure<CParticleEmitter>();

    em.emitting = (g.state != CGrave::State::Destroyed);
}

}  // namespace grave_fx

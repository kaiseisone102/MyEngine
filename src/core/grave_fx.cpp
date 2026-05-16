// =============================================================================
// core/grave_fx.cpp — 墓専用パーティクル制御 (青い蛍)
// =============================================================================
// grave_spirit モデルは origin が墓の上端 (top) にあるため、
// 「足元」 は localOffset.y を負方向、 「上端」 は y=0 付近。
//
// 形状:
//   shape: Box (墓周辺の広がり領域)
//   shapeParams = half extents = (1.2, 1.0, 1.2)
//     → XZ ±1.2m、 Y ±1.0m の範囲で発生 (墓本体を含み外側にも広がる)
//   localOffset: y = -0.5 (墓中心あたりに発生領域の中心を置く)
//
// 速度:
//   emitDirectionLocal: 上向き
//   speed: 0.4 〜 0.8 m/s (ふわふわ上昇)
//   cone: 0.9 (広い散らばり、 揺れ感)
//
// 寿命とサイズ:
//   lifetime: 2.0 〜 3.5 秒 (長め、 ゆっくり上昇する間に上空へ流れる)
//   emitRate: 1.5/s (少なめ、 同時 3-5 個維持)
//   size: 0.08-0.12 → 0.16-0.20 (拡散しながら消える)
//
// 重力:
//   gravity: 0.5 (微かな上向き浮力、 ふわっと上昇感)
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

    // origin が墓の上端なので、 -0.5 で墓中心に発生域中心を置く
    em.localOffset = glm::translate(glm::mat4(1.f), glm::vec3{0.f, -0.5f, 0.f});

    // Box: 墓本体を覆って外側にも広がる
    em.shape = particle::EmitterShape::Box;
    em.shapeParams = glm::vec3{0.4f, 0.4f, 0.4f};  // half extents

    em.emitDirectionLocal = glm::vec3{0.f, 1.f, 0.f};
    em.speedMin = 0.4f;
    em.speedMax = 0.8f;
    em.velocityRandomCone = 0.6f;  // 広い散らばり

    em.emitRate = 1.5f;
    em.lifetimeMin = 2.0f;
    em.lifetimeMax = 3.5f;

    em.sizeStartMin = 0.08f;
    em.sizeStartMax = 0.12f;
    em.sizeEndMin = 0.16f;
    em.sizeEndMax = 0.20f;

    em.colorStart = glm::vec4{0.4f, 0.8f, 1.0f, 0.8f};
    em.colorEnd = glm::vec4{0.1f, 0.3f, 1.0f, 0.0f};

    em.gravity = glm::vec3{0.f, 0.5f, 0.f};  // 微かな上向き浮力
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

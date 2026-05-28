// =============================================================================
// particle_system.cpp — パーティクル + 距離カリング実装
// =============================================================================
#define NOMINMAX
#include "systems/particle_system.h"

#include <algorithm>
#include <cmath>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/components.h"
#include "core/game_state.h"
#include "renderer/animator.h"

ParticleSystem::ParticleSystem() : rng_(std::random_device{}()) {
    pool_.resize(kMaxParticles);
    for (auto& p : pool_) p.alive = false;
}

float ParticleSystem::randFloat(float lo, float hi) {
    if (hi <= lo) return lo;
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

int ParticleSystem::allocateSlot() {
    for (size_t i = 0; i < pool_.size(); ++i) {
        if (!pool_[i].alive) return static_cast<int>(i);
    }
    return -1;
}

glm::vec3 ParticleSystem::sampleShape(const CParticleEmitter& em) {
    using particle::EmitterShape;
    switch (em.shape) {
        case EmitterShape::Point:
            return glm::vec3{0.f};
        case EmitterShape::Sphere: {
            const float r = em.shapeParams.x;
            const float theta = randFloat(0.f, 6.2831853f);
            const float phi = std::acos(randFloat(-1.f, 1.f));
            const float rad = std::cbrt(randFloat(0.f, 1.f)) * r;
            return glm::vec3{
                rad * std::sin(phi) * std::cos(theta),
                rad * std::cos(phi),
                rad * std::sin(phi) * std::sin(theta)
            };
        }
        case EmitterShape::Line: {
            const float t = randFloat(0.f, 1.f);
            return em.shapeParams * t;
        }
        case EmitterShape::Cone: {
            const float r = em.shapeParams.x;
            const float h = em.shapeParams.y;
            const float t = randFloat(0.f, 1.f);
            const float curR = r * t;
            const float angle = randFloat(0.f, 6.2831853f);
            return glm::vec3{
                curR * std::cos(angle),
                h * t,
                curR * std::sin(angle)
            };
        }
        case EmitterShape::Box: {
            return glm::vec3{
                randFloat(-em.shapeParams.x, em.shapeParams.x),
                randFloat(-em.shapeParams.y, em.shapeParams.y),
                randFloat(-em.shapeParams.z, em.shapeParams.z)
            };
        }
    }
    return glm::vec3{0.f};
}

glm::vec3 ParticleSystem::randomDirInCone(const glm::vec3& center, float coneRadius) {
    const float len = glm::length(center);
    if (len < 1e-5f) {
        const float theta = randFloat(0.f, 6.2831853f);
        const float phi = std::acos(randFloat(-1.f, 1.f));
        return glm::vec3{
            std::sin(phi) * std::cos(theta),
            std::cos(phi),
            std::sin(phi) * std::sin(theta)
        };
    }
    const glm::vec3 centerN = center / len;
    if (coneRadius < 1e-5f) return centerN;

    glm::vec3 ortho1;
    if (std::abs(centerN.y) < 0.9f) {
        ortho1 = glm::normalize(glm::cross(centerN, glm::vec3{0.f, 1.f, 0.f}));
    } else {
        ortho1 = glm::normalize(glm::cross(centerN, glm::vec3{1.f, 0.f, 0.f}));
    }
    const glm::vec3 ortho2 = glm::cross(centerN, ortho1);

    const float angle = randFloat(0.f, 6.2831853f);
    const float r = randFloat(0.f, coneRadius);
    const glm::vec3 offset = ortho1 * (r * std::cos(angle)) + ortho2 * (r * std::sin(angle));
    return glm::normalize(centerN + offset);
}

void ParticleSystem::emitFromEmitter(const CParticleEmitter& em, CParticleEmitter& emMut,
                                       const glm::vec3& emitterPos,
                                       const glm::vec3& emitterDirWorld, float dt) {
    if (!em.emitting) return;
    if (em.emitRate <= 0.f) return;

    emMut.accumulator += dt * em.emitRate;
    while (emMut.accumulator >= 1.f) {
        emMut.accumulator -= 1.f;

        int slot = allocateSlot();
        if (slot < 0) break;

        particle::Particle& p = pool_[slot];
        p.alive = true;
        p.age = 0.f;
        p.lifetime = randFloat(em.lifetimeMin, em.lifetimeMax);

        const glm::vec3 shapeOffset = sampleShape(em);
        p.pos = emitterPos + shapeOffset;

        const glm::vec3 dir = randomDirInCone(emitterDirWorld, em.velocityRandomCone);
        const float speed = randFloat(em.speedMin, em.speedMax);
        p.vel = dir * speed;

        p.gravity = em.gravity;
        p.drag = em.drag;

        p.colorStart = em.colorStart;
        p.colorEnd = em.colorEnd;
        p.sizeStart = randFloat(em.sizeStartMin, em.sizeStartMax);
        p.sizeEnd = randFloat(em.sizeEndMin, em.sizeEndMax);
        p.blendMode = em.blendMode;
    }
}

void ParticleSystem::update(WorldData& wd, float dt) {
    // ─── 1. emit: CParticleEmitter を持つ全 entity から ───────
    wd.world.each([&](flecs::entity e, const CTransform& t, CParticleEmitter& em) {
        glm::vec3 emitterPos;
        glm::vec3 emitterDirWorld;

        const bool useBone = (em.attachBoneIdx >= 0) && e.has<CSkeletalAnim>();
        if (useBone) {
            const CSkeletalAnim& sa = e.get<CSkeletalAnim>();
            if (sa.animator.ready()) {
                const glm::mat4 entityWorld = t.matrix();
                const glm::mat4 boneWorld = sa.animator.boneWorldTransform(em.attachBoneIdx);
                const glm::mat4 fullMat = entityWorld * boneWorld * em.localOffset;
                emitterPos = glm::vec3(fullMat[3]);
                emitterDirWorld = glm::normalize(glm::mat3(fullMat) * em.emitDirectionLocal);
            } else {
                emitterPos = t.pos + glm::vec3(em.localOffset[3]);
                emitterDirWorld = em.emitDirectionLocal;
            }
        } else {
            const float yawRad = glm::radians(t.yaw);
            const float cosYaw = std::cos(yawRad);
            const float sinYaw = std::sin(yawRad);
            const glm::vec3& d = em.emitDirectionLocal;
            emitterDirWorld = glm::vec3{
                d.x * cosYaw + d.z * sinYaw,
                d.y,
                -d.x * sinYaw + d.z * cosYaw
            };
            const glm::vec3 offsetLocal = glm::vec3(em.localOffset[3]);
            const glm::vec3 offsetWorld{
                offsetLocal.x * cosYaw + offsetLocal.z * sinYaw,
                offsetLocal.y,
                -offsetLocal.x * sinYaw + offsetLocal.z * cosYaw
            };
            emitterPos = t.pos + offsetWorld;
        }

        // ─── 距離カリング ──────────────────────────────────
        // emitter がカメラから cullingDistance_ より遠ければ emit スキップ。
        // Y 成分は半重み (SceneRenderer と同じ感覚)。
        if (cullingDistance_ > 0.f) {
            const glm::vec3 d = emitterPos - cameraPos_;
            const float dist = std::sqrt(d.x * d.x + d.y * d.y * 0.25f + d.z * d.z);
            if (dist > cullingDistance_) {
                return;  // この emitter の今フレーム emit は無効、 simulate は続く
            }
        }

        emitFromEmitter(em, em, emitterPos, emitterDirWorld, dt);
    });

    // ─── 2. simulate: 全 particle ─────────────────────────────
    // simulate は距離関係なく全粒子。 emit 停止だけで空気感維持。
    aliveCount_ = 0;
    for (auto& p : pool_) {
        if (!p.alive) continue;

        p.age += dt;
        if (p.age >= p.lifetime) {
            p.alive = false;
            continue;
        }

        p.vel += p.gravity * dt;
        if (p.drag > 0.f) {
            const float k = std::max(0.f, 1.f - p.drag * dt);
            p.vel *= k;
        }
        p.pos += p.vel * dt;

        aliveCount_++;
    }
}

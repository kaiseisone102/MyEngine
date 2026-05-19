#pragma once
// =============================================================================
// systems/particle_system.h — パーティクル + 距離カリング
// =============================================================================
// 追加:
//   setCullingParams(cameraPos, distance) — emitter のカリング設定。
//     update() 内で各 emitter の位置とカメラの距離を比較し、
//     distance 超過の emitter からは emit を行わない。
//     既に空中にある粒子は通常通り simulate して寿命で消える。
//     distance <= 0 でカリング無効 (= 全 emitter から emit する、 既存挙動)。
// =============================================================================

#include <flecs.h>

#include <random>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/particle.h"

struct WorldData;
struct CTransform;
struct CParticleEmitter;
struct CSkeletalAnim;

class ParticleSystem {
   public:
    static constexpr size_t kMaxParticles = 2048;

    ParticleSystem();

    // 毎フレーム: emitter から emit + 全 particle update。
    void update(WorldData& wd, float dt);

    // カリングパラメータ設定。 毎フレーム update 前に呼ぶ想定。
    // distance <= 0 でカリング無効 (全 emitter 有効)。
    void setCullingParams(const glm::vec3& cameraPos, float distance) {
        cameraPos_ = cameraPos;
        cullingDistance_ = distance;
    }

    const std::vector<particle::Particle>& particles() const { return pool_; }
    size_t aliveCount() const { return aliveCount_; }

   private:
    std::vector<particle::Particle> pool_;
    size_t aliveCount_ = 0;
    std::mt19937 rng_;

    glm::vec3 cameraPos_{0.f};
    float cullingDistance_ = -1.f;  // 負値 = カリング無効

    void emitFromEmitter(const CParticleEmitter& em, CParticleEmitter& emMut,
                         const glm::vec3& emitterPos, const glm::vec3& emitterDirWorld,
                         float dt);

    glm::vec3 sampleShape(const CParticleEmitter& em);
    glm::vec3 randomDirInCone(const glm::vec3& center, float coneRadius);
    int allocateSlot();
    float randFloat(float lo, float hi);
};

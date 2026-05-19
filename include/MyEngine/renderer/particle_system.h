#pragma once
// =============================================================================
// particle_system.h — CEmitter 更新 + 粒子配列管理
// =============================================================================
// 1. CEmitter::active な entity ごとに accumulator に rate*dt を足し、
//    1.0 超えたら spawn (整数個)
// 2. 既存粒子の lifeRemaining -= dt、 <=0 で削除
// 3. 重力 + 簡易物理で粒子を動かす
// =============================================================================

#include <vector>

#include "core/particle.h"

struct WorldData;

class ParticleSystem {
   public:
    void update(WorldData& wd, float dt);

    const std::vector<particle::Particle>& particles() const { return particles_; }
    void clearAll() { particles_.clear(); }

   private:
    std::vector<particle::Particle> particles_;
};

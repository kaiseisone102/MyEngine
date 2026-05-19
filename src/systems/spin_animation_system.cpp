// =============================================================================
// spin_animation_system.cpp
// =============================================================================
#include "systems/spin_animation_system.h"

#include "core/components.h"
#include "core/game_state.h"

void SpinAnimationSystem::update(WorldData& data, float dt) {
    data.world.each([&](flecs::entity e, CTransform& t, const CSpin& spin) {
        (void)e;
        t.yaw += spin.speedDegPerSec * dt;
        // yaw のラップは不要 (matrix() 内で glm::radians してから rotate するため
        // 大きな値でも数値精度は十分)
    });
}

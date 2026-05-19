// =============================================================================
// systems/gate_system.cpp — + SoundManager (開き始め + 開き切り)
// =============================================================================
#define NOMINMAX
#include "systems/gate_system.h"
#include <cmath>
#include <iostream>
#include <limits>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "core/components.h"
#include "core/game_state.h"
#include "core/gate.h"
#include "core/key.h"
#include "systems/sound_manager.h"
namespace {
glm::vec3 rotateY(const glm::vec3& v, float yawDeg) {
    const float r = yawDeg * 0.017453293f;
    const float c = std::cos(r);
    const float s = std::sin(r);
    return glm::vec3{
        v.x * c + v.z * s,
        v.y,
        -v.x * s + v.z * c
    };
}
void updateGateTransform(CTransform& t, const CGate& g) {
    if (g.openMode == CGate::OpenMode::Slide) {
        t.pos = g.closedPos + g.openOffset * g.progress;
        return;
    }
    const float currentYaw = g.closedYaw + g.openYawDelta * g.progress;
    const glm::vec3 hingeWorld = g.closedPos + rotateY(g.hingeOffsetLocal, g.closedYaw);
    const glm::vec3 hingeToCenter = rotateY(-g.hingeOffsetLocal, currentYaw);
    t.pos = hingeWorld + hingeToCenter;
    t.yaw = currentYaw;
}
bool playerHasRequiredKey(WorldData& wd, const CGate& g) {
    if (!g.requiresKey) return true;
    if (!wd.player || !wd.player.is_alive()) return false;
    if (!wd.player.has<CKeyInventory>()) return false;
    return wd.player.get<CKeyInventory>().has(g.requiredKey);
}
}  // namespace
void GateSystem::update(WorldData& wd, float dt, SoundManager& sound) const {
    // グループ gate のうち最初に Opening → Open 遷移した 1 つだけで音を鳴らす
    // (= group 全部が同時遷移する場合に N 回連続で重ねないため)
    bool playedFinishedSoundThisFrame = false;

    wd.world.each([&](flecs::entity, CTransform& t, CGate& g) {
        if (g.state != CGate::State::Opening) return;
        g.progress += dt / g.duration;
        if (g.progress >= 1.f) {
            g.progress = 1.f;
            g.state = CGate::State::Open;
            // 開き切り SE (グループ単位で 1 回)
            if (!playedFinishedSoundThisFrame) {
                sound.playGateOpen();
                playedFinishedSoundThisFrame = true;
            }
        }
        updateGateTransform(t, g);
    });
}
flecs::entity GateSystem::findNearestOpenableGate(WorldData& wd) const {
    if (!wd.player || !wd.player.is_alive()) return flecs::entity::null();
    if (!wd.player.has<CTransform>()) return flecs::entity::null();
    const glm::vec3& playerPos = wd.player.get<CTransform>().pos;
    flecs::entity bestGate = flecs::entity::null();
    float bestDistSq = std::numeric_limits<float>::max();
    wd.world.each([&](flecs::entity e, const CTransform& t, const CGate& g) {
        if (!g.isOpenable()) return;
        const float dx = playerPos.x - t.pos.x;
        const float dy = playerPos.y - t.pos.y;
        const float dz = playerPos.z - t.pos.z;
        const float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > g.interactRange * g.interactRange) return;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestGate = e;
        }
    });
    return bestGate;
}
GateSystem::OpenResult GateSystem::tryOpenNearestGate(WorldData& wd, SoundManager& sound) const {
    flecs::entity gate = findNearestOpenableGate(wd);
    if (!gate || !gate.is_alive()) return OpenResult::NoGate;
    CGate& g = gate.ensure<CGate>();
    if (!g.isOpenable()) return OpenResult::NoGate;
    if (!playerHasRequiredKey(wd, g)) {
        std::cout << "[Gate] '" << gate.name().c_str() << "' needs "
                  << keyTypeName(g.requiredKey) << " key\n";
        return OpenResult::NeedsKey;
    }
    if (g.requiresKey && wd.player.has<CKeyInventory>()) {
        CKeyInventory& inv = wd.player.ensure<CKeyInventory>();
        if (inv.consume(g.requiredKey)) {
            std::cout << "[Key] consumed " << keyTypeName(g.requiredKey)
                      << " key, remaining=" << inv.count(g.requiredKey) << "\n";
        }
    }
    if (g.groupId == 0) {
        g.state = CGate::State::Opening;
        g.progress = 0.f;
        std::cout << "[Gate] opening '" << gate.name().c_str() << "' (solo)\n";
        sound.playGateOpen();  // 開き始め SE
        return OpenResult::Opened;
    }
    const int targetGroup = g.groupId;
    int openedCount = 0;
    wd.world.each([&](flecs::entity e, CGate& gg) {
        (void)e;
        if (gg.groupId != targetGroup) return;
        if (!gg.isOpenable()) return;
        gg.state = CGate::State::Opening;
        gg.progress = 0.f;
        ++openedCount;
    });
    std::cout << "[Gate] opening group " << targetGroup << " (" << openedCount
              << " gates)\n";
    sound.playGateOpen();  // 開き始め SE (グループでも 1 回)
    return OpenResult::Opened;
}

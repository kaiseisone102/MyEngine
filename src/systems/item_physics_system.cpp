// =============================================================================
// systems/item_physics_system.cpp — + potionItems 物理対応
// =============================================================================
// 着地時に CPickupCooldown を 1 秒で設定 → ItemPickupSystem 側が拾い無効化。
// 毎フレーム cooldown を dec して 0 になったら削除。
// =============================================================================
#define NOMINMAX
#include "systems/item_physics_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/aabb.h"
#include "core/chest.h"
#include "core/components.h"
#include "core/game_state.h"
#include "renderer/terrain_mesh.h"
#include "systems/physics_util.h"
#include "world/world_terrain.h"

namespace {

constexpr float kPickupCooldownAfterLanding = 1.0f;

bool circleAABBOverlapXZ(float cx, float cz, float r, const AABB& box) {
    const float nx = std::max(box.min.x, std::min(cx, box.max.x));
    const float nz = std::max(box.min.z, std::min(cz, box.max.z));
    const float dx = cx - nx;
    const float dz = cz - nz;
    return (dx * dx + dz * dz) <= (r * r);
}

float findGroundY(const glm::vec3& itemPos, const std::vector<flecs::entity>& platforms,
                  const WorldTerrain* terrain) {
    float bestY = std::numeric_limits<float>::lowest();

    if (terrain) {
        const float th = terrain->sampleHeight(itemPos.x, itemPos.z);
        if (th != std::numeric_limits<float>::lowest()) {
            bestY = std::max(bestY, th);
        }
    }

    constexpr float kItemRadius = 0.3f;
    for (flecs::entity plat : platforms) {
        if (!plat.is_alive()) continue;
        const AABB platBox = physics::entityAABB(plat);
        if (!circleAABBOverlapXZ(itemPos.x, itemPos.z, kItemRadius, platBox)) continue;
        if (itemPos.y >= platBox.max.y - 0.1f) {
            bestY = std::max(bestY, platBox.max.y);
        }
    }

    return bestY;
}

}  // namespace

void ItemPhysicsSystem::update(WorldData& wd, float dt, float gravity) const {
    // 1. 飛行中アイテムの物理 + 着地
    auto stepItem = [&](flecs::entity item) {
        if (!item || !item.is_alive()) return;
        if (!item.has<CVelocity>() || !item.has<CTransform>()) return;

        CTransform& t = item.ensure<CTransform>();
        CVelocity& v = item.ensure<CVelocity>();

        v.y += gravity * dt;
        if (v.y < -30.f) v.y = -30.f;

        constexpr float kDrag = 2.0f;
        const float dragFactor = std::max(0.f, 1.f - kDrag * dt);
        v.xz *= dragFactor;

        t.pos.x += v.xz.x * dt;
        t.pos.y += v.y * dt;
        t.pos.z += v.xz.y * dt;

        const float groundY = findGroundY(t.pos, wd.platforms, &wd.terrains);
        if (groundY != std::numeric_limits<float>::lowest() && t.pos.y <= groundY) {
            t.pos.y = groundY;
            v.y = 0.f;
            v.xz = glm::vec2{0.f};
            // 着地完了 → CVelocity 削除 + 拾いクールダウン設定
            item.remove<CVelocity>();
            item.set<CPickupCooldown>({kPickupCooldownAfterLanding});
        }
    };

    for (flecs::entity item : wd.moneyItems)  stepItem(item);
    for (flecs::entity item : wd.armorItems)  stepItem(item);
    for (flecs::entity item : wd.keyItems)    stepItem(item);
    for (flecs::entity item : wd.gripItems)   stepItem(item);
    for (flecs::entity item : wd.potionItems) stepItem(item);

    // 2. 着地済みアイテムのクールダウン減算
    auto tickCooldown = [&](flecs::entity item) {
        if (!item || !item.is_alive()) return;
        if (!item.has<CPickupCooldown>()) return;

        CPickupCooldown& cd = item.ensure<CPickupCooldown>();
        cd.remaining -= dt;
        if (cd.remaining <= 0.f) {
            item.remove<CPickupCooldown>();
        }
    };

    for (flecs::entity item : wd.moneyItems)  tickCooldown(item);
    for (flecs::entity item : wd.armorItems)  tickCooldown(item);
    for (flecs::entity item : wd.keyItems)    tickCooldown(item);
    for (flecs::entity item : wd.gripItems)   tickCooldown(item);
    for (flecs::entity item : wd.potionItems) tickCooldown(item);
}

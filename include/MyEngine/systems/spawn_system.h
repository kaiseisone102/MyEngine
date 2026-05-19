#pragma once
// =============================================================================
// spawn_system.h — Phase 4 段階4-3
// =============================================================================
// 段階4-3 変更:
//   敵タイプごとに別モデルを bind できるよう、attachSkeletalAnim を
//   モデル名引数化。後方互換のため attachKnightSkeletalAnim も残す。
// =============================================================================

#include <flecs.h>

#include <vector>

#include "core/spawn_trigger.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

class VulkanRenderer;

namespace EnemySpawnDefaults {
constexpr float kSkeletonAttackStartupLag = 0.20f;
}

class SpawnSystem {
   public:
    static flecs::entity createSkeleton(flecs::world& world, const char* entityName,
                                        const glm::vec3& pos,
                                        std::vector<flecs::entity>* outEnemies,
                                        float spawnStunSeconds = 0.f,
                                        VulkanRenderer* vulkan = nullptr);

    void update(std::vector<SpawnTrigger>& triggers, flecs::entity player,
                std::vector<flecs::entity>& enemies, flecs::world& world,
                VulkanRenderer& vulkan) const;

    // Phase 4 段階4-3: モデル名指定版 (skeleton, soldier 等)
    static void attachSkeletalAnim(flecs::entity e, VulkanRenderer& vulkan,
                                   const char* modelName);

    // 後方互換: knight 固定 (Player 用)
    static void attachKnightSkeletalAnim(flecs::entity e, VulkanRenderer& vulkan);
};

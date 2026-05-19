#define NOMINMAX
// =============================================================================
// spawn_system.cpp — Phase 4 段階4-5C
// 敵 spawn 時の初期 bind を enemy_idle.glb に変更 (なければ idle.glb)
// =============================================================================

#include "systems/spawn_system.h"

#include <iostream>
#include <utility>

#include "core/components.h"
#include "renderer/animator.h"
#include "renderer/asset_registry.h"
#include "renderer/model.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/vulkan_renderer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <string>

static int s_spawnIndex = 100;

void SpawnSystem::attachSkeletalAnim(flecs::entity e, VulkanRenderer& vulkan,
                                     const char* modelName) {
    AssetRegistry& assets = vulkan.assets();

    const Model* model = assets.getModel(modelName);
    const char* usedModel = modelName;
    if (!model || !model->hasSkeleton()) {
        std::cerr << "[SpawnSystem] model '" << modelName
                  << "' not available, falling back to 'knight'\n";
        model = assets.getModel("knight");
        usedModel = "knight";
        if (!model || !model->hasSkeleton()) {
            std::cerr << "[SpawnSystem] knight fallback also failed; skipping skinning\n";
            return;
        }
    }

    // Phase 4 段階4-5C: 敵専用 idle を優先、なければ Player 用 idle にフォールバック
    const AnimationClip* clip = assets.getAnimation("enemy_idle");
    if (!clip) clip = assets.getAnimation("idle");
    if (!clip) clip = assets.getAnimation("walk");

    CSkeletalAnim sa;
    sa.model = model;
    sa.animator.bind(&model->skeleton(), clip);
    sa.skinMatrices.assign(model->skeleton().boneCount(), glm::mat4(1.f));
    sa.playing = true;
    sa.speed = 1.f;
    sa.skinSlot = vulkan.skinBufferPool().allocate();

    if (!sa.skinSlot.valid()) {
        std::cerr << "[SpawnSystem] WARNING: SkinBufferPool full, enemy will not be skinned\n";
        return;
    }

    e.set<CSkeletalAnim>(std::move(sa));

    CAnimState as;
    as.current = AnimState::Idle;
    as.previous = AnimState::Idle;
    e.set<CAnimState>(std::move(as));

    (void)usedModel;
}

void SpawnSystem::attachKnightSkeletalAnim(flecs::entity e, VulkanRenderer& vulkan) {
    attachSkeletalAnim(e, vulkan, "knight");
}

flecs::entity SpawnSystem::createSkeleton(flecs::world& world, const char* entityName,
                                          const glm::vec3& pos,
                                          std::vector<flecs::entity>* outEnemies,
                                          float spawnStunSeconds, VulkanRenderer* vulkan) {
    flecs::entity e = world.entity(entityName);
    e.set<CTransform>({pos, 0.f, {0.6f, 1.0f, 0.6f}})
        .set<CEnemyAI>({})
        .set<CVelocity>({0.f})
        .set<CHealth>({1, 1})
        .add<EnemyTag>()
        .add<SkeletonTag>();
    auto& ai = e.ensure<CEnemyAI>();
    ai.spawnStunTimer = spawnStunSeconds;
    ai.attackStartupLag = EnemySpawnDefaults::kSkeletonAttackStartupLag;

    if (vulkan) {
        attachSkeletalAnim(e, *vulkan, "skeleton");
    }

    if (outEnemies) outEnemies->push_back(e);
    return e;
}

static void spawnEnemy(EnemyType type, const glm::vec3& pos, flecs::world& world,
                       std::vector<flecs::entity>& enemies, VulkanRenderer& vulkan) {
    const std::string name = "spawned_" + std::to_string(s_spawnIndex++);

    if (type == EnemyType::Skeleton) {
        SpawnSystem::createSkeleton(world, name.c_str(), pos, &enemies, 2.f, &vulkan);
        return;
    }

    flecs::entity e = world.entity(name.c_str());

    if (type == EnemyType::Ghost) {
        e.set<CTransform>({pos, 0.f, {0.5f, 0.8f, 0.5f}})
            .set<CEnemyAI>({EnemyState::Chase, 3.5f})
            .set<CHealth>({1, 1})
            .add<EnemyTag>()
            .add<GhostTag>();
        auto& ai = e.ensure<CEnemyAI>();
        ai.spawnStunTimer = 4.f;
    } else {
        e.set<CTransform>({pos, 0.f, {0.6f, 1.0f, 0.6f}})
            .set<CEnemyAI>({EnemyState::Chase, 2.2f, 1.9f, 0.35f, 1.10f})
            .set<CVelocity>({0.f})
            .set<CHealth>({1, 2})
            .add<EnemyTag>()
            .add<SoldierTag>();
        auto& ai = e.ensure<CEnemyAI>();
        ai.spawnStunTimer = 1.f;
        ai.attackStartupLag = 0.10f;

        SpawnSystem::attachSkeletalAnim(e, vulkan, "soldier");
    }

    enemies.push_back(e);
}

void SpawnSystem::update(std::vector<SpawnTrigger>& triggers, flecs::entity player,
                         std::vector<flecs::entity>& enemies, flecs::world& world,
                         VulkanRenderer& vulkan) const {
    const glm::vec3& pPos = player.get<CTransform>().pos;

    for (SpawnTrigger& trig : triggers) {
        if (trig.triggered) continue;

        const glm::vec2 diff{pPos.x - trig.center.x, pPos.z - trig.center.z};
        if (glm::length(diff) > trig.radius) continue;

        for (const SpawnEntry& entry : trig.spawns) {
            spawnEnemy(entry.type, entry.position, world, enemies, vulkan);
        }

        trig.triggered = true;
    }
}

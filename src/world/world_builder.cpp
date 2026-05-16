// world/world_builder.cpp
// =============================================================================
// + reset() で spirits / graves destruct
// + buildPlayer で CSpirit 付与
// =============================================================================
#include "world/world_builder.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <utility>

#include "core/components.h"
#include "core/equipment_util.h"
#include "core/grip.h"
#include "core/spirit.h"
#include "renderer/animator.h"
#include "renderer/model.h"
#include "renderer/model_scale_registry.h"
#include "renderer/skeleton.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/vulkan_renderer.h"
#include "systems/spawn_system.h"
#include "world/stage_def.h"

void WorldBuilder::build(WorldData& data, StageId stageId, bool keepPlayer) {
    static bool s_onRemoveRegistered = false;
    if (!s_onRemoveRegistered) {
        data.world.component<CSkeletalAnim>().on_remove(
            [&data](flecs::entity e, CSkeletalAnim& sa) {
                (void)e;
                if (sa.skinSlot.valid()) {
                    data.vulkan.skinBufferPool().release(sa.skinSlot);
                    sa.skinSlot = SkinBufferPool::Slot::invalid();
                }
            });
        s_onRemoveRegistered = true;
    }

    const StageDef& def = stage_registry::get(stageId);
    std::cout << "[WorldBuilder] build stage: " << def.name
              << " (keepPlayer=" << (keepPlayer ? "true" : "false") << ")\n";

    if (keepPlayer) {
        if (data.player && data.player.is_alive()) {
            auto& pt = data.player.ensure<CTransform>();
            pt.pos = def.playerSpawnPos;

            auto& pv = data.player.ensure<CVelocity>();
            pv.y = 0.f;

            auto& pp = data.player.ensure<CPhysics>();
            pp.onGround = false;
            pp.jumpReq = false;
            pp.jumpsRemaining = pp.maxJumps;
            pp.usedDoubleJump = false;

            if (data.player.has<CAttack>()) {
                auto& atk = data.player.ensure<CAttack>();
                atk.def = nullptr;
                atk.elapsed = 0.f;
                atk.isAerial = false;
                atk.isDiving = false;
                atk.diveDropStarted = false;
                atk.hitEntities.clear();
                atk.prevSweepWorldDir = glm::vec3{0.f};
                atk.landingShockwaveFired = false;
                atk.gripConsumedThisAttack = false;
            }

            if (data.player.has<CShield>()) {
                data.player.ensure<CShield>().guarding = false;
            }

            std::cout << "[WorldBuilder] player kept (pos=" << pt.pos.x << "," << pt.pos.y << ","
                      << pt.pos.z << ")\n";
        } else {
            std::cerr << "[WorldBuilder] WARNING: keepPlayer=true but player is null, "
                         "falling back to buildPlayer\n";
            buildPlayer(data, def.playerSpawnPos);
        }
    } else {
        buildPlayer(data, def.playerSpawnPos);
    }

    if (def.buildContent) {
        def.buildContent(data);
    } else {
        std::cerr << "[WorldBuilder] WARNING: stage '" << def.name
                  << "' has no buildContent function\n";
    }
}

void WorldBuilder::reset(WorldData& data, bool keepPlayer) {
    auto destructAll = [](std::vector<flecs::entity>& v) {
        for (flecs::entity e : v) {
            if (e.is_alive()) e.destruct();
        }
        v.clear();
    };

    if (!keepPlayer) {
        if (data.player && data.player.is_alive()) {
            data.player.destruct();
        }
        data.player = flecs::entity::null();
    }

    destructAll(data.enemies);
    destructAll(data.platforms);
    destructAll(data.shieldItems);
    destructAll(data.armorItems);
    destructAll(data.gripItems);
    destructAll(data.decorations);
    destructAll(data.gates);
    destructAll(data.keyItems);
    destructAll(data.moneyItems);
    destructAll(data.spiritItems);
    destructAll(data.chests);
    destructAll(data.graves);

    // obstacles は decorations / chests / graves と重複しているため
    // ここでは destructAll せず vector.clear() のみ。
    data.obstacles.clear();

    {
        std::vector<flecs::entity> warpPads;
        data.world.each([&](flecs::entity e, const CWarpPad&) { warpPads.push_back(e); });
        for (flecs::entity e : warpPads) {
            if (e.is_alive()) e.destruct();
        }
    }

    data.spawnTriggers.clear();

    if (!data.terrains.empty()) {
        vkDeviceWaitIdle(data.vulkan.context().device());
        data.terrains.clear();
    }

    std::cout << "[WorldBuilder] reset done"
              << (keepPlayer ? " (player kept)" : " (player destroyed)")
              << ". Pool: " << data.vulkan.skinBufferPool().allocatedCount() << "/"
              << SkinBufferPool::MAX_ENTITIES << "\n";
}

void WorldBuilder::buildPlayer(WorldData& data, const glm::vec3& spawnPos) {
    data.player = data.world.entity("player")
                      .set<CTransform>({spawnPos, 0.f, {0.6f, 1.f, 0.6f}})
                      .set<CVelocity>({0.f})
                      .set<CPhysics>({false, false, 5.f})
                      .set<CAttack>({})
                      .set<CHealth>({})
                      .set<CShield>({ShieldType::Iron, CShield::maxDurability(ShieldType::Iron)})
                      .set<CAnimState>({})
                      .set<CGrip>({})
                      .set<CSpirit>({})  // 初期: amount=0
                      .add<PlayerTag>();

    AssetRegistry& assets = data.vulkan.assets();
    const Model* knightModel = assets.getModel("knight");

    if (knightModel && knightModel->hasSkeleton()) {
        const AnimationClip* clip = assets.getAnimation("idle");
        const char* usedClipName = "idle";
        if (!clip) {
            clip = assets.getAnimation("walk");
            usedClipName = "walk";
        }
        if (!clip) {
            clip = assets.getAnimation("knight");
            usedClipName = "knight (T-Pose)";
        }

        CSkeletalAnim sa;
        sa.model = knightModel;
        sa.animator.bind(&knightModel->skeleton(), clip);
        sa.skinMatrices.assign(knightModel->skeleton().boneCount(), glm::mat4(1.f));
        sa.playing = true;
        sa.speed = 1.f;
        sa.skinSlot = data.vulkan.skinBufferPool().allocate();
        if (!sa.skinSlot.valid()) {
            std::cerr << "[WorldBuilder] WARNING: SkinBufferPool allocation failed for player\n";
        } else {
            std::cout << "[WorldBuilder] Player skinSlot: offset=" << sa.skinSlot.boneOffset
                      << " capacity=" << sa.skinSlot.boneCapacity << "\n";
        }

        data.player.set<CSkeletalAnim>(std::move(sa));

        std::cout << "[WorldBuilder] Player initial clip='" << usedClipName << "'\n";

        setupPlayerEquipment(data, knightModel);
    } else {
        std::cout << "[WorldBuilder] knight model not available, skipping CSkeletalAnim\n";
    }
}

void WorldBuilder::setupPlayerEquipment(WorldData& data, const Model* knightModel) {
    AssetRegistry& assets = data.vulkan.assets();
    const Skeleton& sk = knightModel->skeleton();

    const Model* swordModel = assets.getModel("sword_std");
    if (!swordModel || swordModel->empty()) {
        std::cerr << "[WorldBuilder] CEquipment: sword_std not available\n";
    }

    const int leftBoneIdx =
        sk.findBoneByAnyName({"mixamorig:Shield_joint", "Shield_joint", "shield_attach",
                              "mixamorig:LeftHand", "LeftHand"});
    const int rightBoneIdx =
        sk.findBoneByAnyName({"mixamorig:Sword_joint", "Sword_joint", "sword_attach",
                              "mixamorig:RightHand", "RightHand"});

    const glm::vec3 swordScale = model_scale::get("sword_std", model_scale::Context::Equipped);

    CEquipment eq;
    eq.leftHandBoneIdx = leftBoneIdx;
    eq.leftHandModel = nullptr;
    eq.leftHandLocalOffset = glm::mat4(1.f);
    eq.leftHandScale = {1.f, 1.f, 1.f};
    eq.leftHandVisible = false;

    eq.rightHandBoneIdx = rightBoneIdx;
    eq.rightHandModel = swordModel;
    eq.rightHandLocalOffset = glm::mat4(1.f);
    eq.rightHandScale = swordScale;
    eq.rightHandVisible = (swordModel != nullptr && !swordModel->empty());

    data.player.set<CEquipment>(eq);

    equipment::applyShieldChange(data.player, assets, ShieldType::Iron);

    if (rightBoneIdx >= 0) {
        CParticleEmitter em;
        em.attachBoneIdx = rightBoneIdx;
        em.localOffset = glm::translate(glm::mat4(1.f), glm::vec3{0.f, 0.5f, 0.f});

        em.shape = particle::EmitterShape::Line;
        em.shapeParams = glm::vec3{0.f, 0.5f, 0.f};

        em.emitting = false;

        em.emitDirectionLocal = glm::vec3{0.f, 1.f, 0.f};
        em.speedMin = 1.5f;
        em.speedMax = 2.5f;
        em.velocityRandomCone = 0.3f;

        em.blendMode = particle::BlendMode::Additive;

        em.emitRate = 0.f;
        em.lifetimeMin = 0.4f;
        em.lifetimeMax = 0.7f;
        em.sizeStartMin = 0.06f;
        em.sizeStartMax = 0.10f;
        em.sizeEndMin = 0.18f;
        em.sizeEndMax = 0.28f;
        em.colorStart = glm::vec4{1.f};
        em.colorEnd = glm::vec4{1.f, 0.f, 0.f, 0.f};
        em.gravity = glm::vec3{0.f, 2.0f, 0.f};
        em.drag = 1.5f;

        data.player.set<CParticleEmitter>(em);

        std::cout << "[WorldBuilder] Player emitter attached (idle, awaiting grip pickup)\n";
    }
}

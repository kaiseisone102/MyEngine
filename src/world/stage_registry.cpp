// =============================================================================
// stage_registry.cpp — Terminal 壁接地 + Stage 1-2 ゲート間隔修正
// =============================================================================
#include <cassert>
#include <glm/glm.hpp>
#include <iostream>
#include <utility>

#include "core/components.h"
#include "core/game_state.h"
#include "core/gate.h"
#include "core/grip.h"
#include "core/spawn_trigger.h"
#include "renderer/material.h"
#include "renderer/model.h"
#include "renderer/model_scale_registry.h"
#include "renderer/resource_factory.h"
#include "renderer/terrain_mesh.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_renderer.h"
#include "systems/spawn_system.h"
#include "world/polygon_helpers.h"
#include "world/stage_def.h"
#include "world/terrain_profiles.h"

namespace {

flecs::entity addPlatform(WorldData& data, const char* name, glm::vec3 pos, glm::vec3 scale,
                          const char* materialName = nullptr) {
    flecs::entity e = data.world.entity(name).set<CTransform>({pos, 0.f, scale}).add<PlatformTag>();
    if (materialName) {
        const Material* mat = data.vulkan.assets().getMaterial(materialName);
        if (mat) {
            e.set<CMaterialRef>({mat});
        } else {
            std::cerr << "[StageRegistry] material not found: " << materialName
                      << " (using default)\n";
        }
    }
    data.platforms.push_back(e);
    return e;
}

flecs::entity addMovingPlatform(WorldData& data, const char* name, glm::vec3 pos, glm::vec3 scale,
                                CMovingPlatform::Pattern pattern, glm::vec3 axis, float amplitude,
                                float angularSpeed, float initialPhase = 0.f,
                                const char* materialName = nullptr) {
    flecs::entity e = addPlatform(data, name, pos, scale, materialName);

    CMovingPlatform mp{};
    mp.pattern = pattern;
    mp.originPos = pos;
    mp.axis = axis;
    mp.amplitude = amplitude;
    mp.angularSpeed = angularSpeed;
    mp.phase = initialPhase;
    e.set<CMovingPlatform>(mp);

    return e;
}

flecs::entity addSlideGate(WorldData& data, const char* name, glm::vec3 pos, glm::vec3 scale,
                           glm::vec3 openOffset, float duration = 1.0f, float interactRange = 3.0f,
                           int groupId = 0, const char* materialName = nullptr) {
    flecs::entity e = addPlatform(data, name, pos, scale, materialName);

    CGate g{};
    g.state = CGate::State::Closed;
    g.openMode = CGate::OpenMode::Slide;
    g.closedPos = pos;
    g.openOffset = openOffset;
    g.duration = duration;
    g.progress = 0.f;
    g.interactRange = interactRange;
    g.groupId = groupId;
    e.set<CGate>(g);
    e.add<GateTag>();

    data.gates.push_back(e);
    return e;
}

flecs::entity addRotateGate(WorldData& data, const char* name, glm::vec3 pos, glm::vec3 scale,
                            glm::vec3 hingeOffsetLocal, float openYawDelta, float duration = 1.0f,
                            float interactRange = 3.0f, int groupId = 0, float closedYaw = 0.f,
                            const char* materialName = nullptr) {
    flecs::entity e =
        data.world.entity(name).set<CTransform>({pos, closedYaw, scale}).add<PlatformTag>();
    if (materialName) {
        const Material* mat = data.vulkan.assets().getMaterial(materialName);
        if (mat) {
            e.set<CMaterialRef>({mat});
        } else {
            std::cerr << "[StageRegistry] material not found: " << materialName
                      << " (using default)\n";
        }
    }
    data.platforms.push_back(e);

    CGate g{};
    g.state = CGate::State::Closed;
    g.openMode = CGate::OpenMode::Rotate;
    g.closedPos = pos;
    g.hingeOffsetLocal = hingeOffsetLocal;
    g.closedYaw = closedYaw;
    g.openYawDelta = openYawDelta;
    g.duration = duration;
    g.progress = 0.f;
    g.interactRange = interactRange;
    g.groupId = groupId;
    e.set<CGate>(g);
    e.add<GateTag>();

    data.gates.push_back(e);
    return e;
}

void addWarpPad(WorldData& data, const char* name, glm::vec3 pos, StageId target,
                float radius = 1.5f) {
    data.world.entity(name)
        .set<CTransform>({pos, 0.f, {2.5f, 0.1f, 2.5f}})
        .set<CWarpPad>({target, radius});
}

void addTerrain(WorldData& data, const std::vector<glm::vec2>& polygonXZ, float baseY,
                const TerrainMesh::HeightFunc& heightFunc, const char* materialName = nullptr,
                float cellSize = 1.5f, float uvScale = 3.f) {
    const Material* mat = nullptr;
    if (materialName) {
        mat = data.vulkan.assets().getMaterial(materialName);
        if (!mat) {
            std::cerr << "[StageRegistry] terrain material not found: " << materialName
                      << " (using default)\n";
        }
    }
    auto mesh = std::make_unique<TerrainMesh>();
    mesh->init(&data.vulkan.context(), &data.vulkan.resources(), polygonXZ, baseY, heightFunc,
               cellSize, uvScale, mat);
    data.terrains.add(std::move(mesh));
}

void addStaircase(WorldData& data, const char* baseName, int steps, glm::vec3 startPos,
                  glm::vec3 stepDelta, float yStep, glm::vec3 stepScale,
                  const char* materialName = nullptr) {
    for (int i = 0; i < steps; ++i) {
        const glm::vec3 pos = startPos + stepDelta * static_cast<float>(i) +
                              glm::vec3{0.f, yStep * static_cast<float>(i), 0.f};
        const std::string name = std::string(baseName) + std::to_string(i);
        addPlatform(data, name.c_str(), pos, stepScale, materialName);
    }
}

void addDecor(WorldData& data, const char* name, glm::vec3 pos, const char* modelName,
              float yaw = 0.f) {
    const Model* model = data.vulkan.assets().getModel(modelName);
    if (!model || model->empty()) {
        std::cerr << "[StageRegistry] decor model not found or empty: " << modelName << "\n";
        return;
    }
    const glm::vec3 scale = model_scale::get(modelName, model_scale::Context::Default);
    flecs::entity e =
        data.world.entity(name).set<CTransform>({pos, yaw, scale}).set<CStaticModelRef>({model});
    data.decorations.push_back(e);
}

void addShieldItem(WorldData& data, const char* name, glm::vec3 pos, ShieldType type) {
    const char* modelName = (type == ShieldType::Iron)     ? "shield_iron"
                            : (type == ShieldType::Silver) ? "shield_silver"
                                                           : "shield_gold";
    const Model* model = data.vulkan.assets().getModel(modelName);

    glm::vec3 itemPos = pos;
    itemPos.y = 0.5f;

    const glm::vec3 scale = model_scale::get(modelName, model_scale::Context::Default);

    auto e = data.world.entity(name)
                 .set<CTransform>({itemPos, 0.f, scale})
                 .set<CPickup>({type})
                 .set<CSpin>({90.f})
                 .add<ShieldItemTag>();

    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
    } else {
        std::cerr << "[StageRegistry] shield model not found: " << modelName
                  << " (fallback to Cube)\n";
    }

    data.shieldItems.push_back(e);
}

void addGripItem(WorldData& data, const char* name, glm::vec3 pos, GripType type) {
    const GripDef& gdef = grip::def(type);
    const Model* model = data.vulkan.assets().getModel(gdef.modelName);

    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty()) {
        scale = model_scale::get(gdef.modelName, model_scale::Context::Default);
    }

    auto e = data.world.entity(name)
                 .set<CTransform>({pos, 0.f, scale})
                 .set<CGripPickup>({type})
                 .set<CSpin>({90.f})
                 .add<GripItemTag>();

    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
    } else {
        std::cerr << "[StageRegistry] grip model not found: " << gdef.modelName
                  << " (fallback to Cube)\n";
    }

    grip::attachItemEmitter(e, type);

    data.gripItems.push_back(e);
}

// ===========================================================================
// Terminal: 壁を接地 (pos.y=0、 高さ 3m)
// ===========================================================================
void buildTerminal(WorldData& data) {
    addTerrain(data, polygon::rectangle({0.f, 0.f}, {50.f, 50.f}), 0.0f, terrain_profile::flat,
               "grass_field");

    // 壁: pos.y=0 (= 地面 Y=0 にぴったり接地)、 高さ 3m、 厚さ 0.5m
    addPlatform(data, "terminal_wall_n", {0.f, 0.f, 18.f}, {30.f, 3.f, 0.5f}, "stone_wall");
    addPlatform(data, "terminal_wall_s", {0.f, 0.f, -18.f}, {30.f, 3.f, 0.5f}, "stone_wall");
    addPlatform(data, "terminal_wall_e", {18.f, 0.f, 0.f}, {0.5f, 3.f, 30.f}, "stone_wall");
    addPlatform(data, "terminal_wall_w", {-18.f, 0.f, 0.f}, {0.5f, 3.f, 30.f}, "stone_wall");

    const auto reals = stage_registry::realStages();
    const int n = static_cast<int>(reals.size());
    constexpr float kRadius = 8.f;
    for (int i = 0; i < n; ++i) {
        const float angle =
            (3.14159265f * 2.f) * static_cast<float>(i) / static_cast<float>(n > 0 ? n : 1);
        const glm::vec3 pos{kRadius * std::cos(angle), 0.f, kRadius * std::sin(angle)};
        const std::string padName = "warp_to_" + std::to_string(static_cast<int>(reals[i]->id));
        addWarpPad(data, padName.c_str(), pos, reals[i]->id, 1.5f);
    }

    SpawnSystem::createSkeleton(data.world, "term_demo_sk0", {0.f, 0.f, 12.f}, &data.enemies, 0.f,
                                &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "term_demo_sk1", {-6.f, 0.f, -10.f}, &data.enemies, 0.f,
                                &data.vulkan);

    addShieldItem(data, "term_shield_iron0", {4.f, 0.f, -2.f}, ShieldType::Iron);

    addGripItem(data, "term_fire_grip0", {-4.f, 0.5f, 4.f}, GripType::Fire);
    addGripItem(data, "term_fire_grip1", {6.f, 0.5f, 6.f}, GripType::Fire);

    addDecor(data, "term_grave_0", {-12.f, 0.f, -8.f}, "grave_1");
    addDecor(data, "term_rock_0", {12.f, 0.f, -8.f}, "rock_1");
    addDecor(data, "term_tree_0", {0.f, 0.f, -14.f}, "tree_noLeaves_1");
}

void buildStage1_1(WorldData& data) {
    addTerrain(data, polygon::rectangle({0.f, 0.f}, {100.f, 100.f}), 0.0f,
               terrain_profile::stage1_1_terrain, "grass_field", /*cellSize=*/1.5f);

    SpawnSystem::createSkeleton(data.world, "s1_1_sk0", {10.f, 0.f, 8.f}, &data.enemies, 0.f,
                                &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "s1_1_sk1", {15.f, 0.f, 5.f}, &data.enemies, 0.f,
                                &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "s1_1_sk2", {12.f, 0.f, 12.f}, &data.enemies, 0.f,
                                &data.vulkan);

    auto addSoldier = [&](const char* name, glm::vec3 pos) {
        flecs::entity e = data.world.entity(name)
                              .set<CTransform>({pos, 0.f, {0.6f, 1.0f, 0.6f}})
                              .set<CEnemyAI>({EnemyState::Chase, 2.0f})
                              .set<CVelocity>({0.f})
                              .set<CHealth>({1, 2})
                              .add<EnemyTag>()
                              .add<SoldierTag>();
        e.ensure<CEnemyAI>().attackStartupLag = 0.10f;
        SpawnSystem::attachSkeletalAnim(e, data.vulkan, "soldier");
        data.enemies.push_back(e);
    };
    addSoldier("s1_1_sol0", {-10.f, 0.f, 8.f});
    addSoldier("s1_1_sol1", {-12.f, 0.f, 12.f});

    {
        SpawnTrigger t;
        t.center = {0.f, 0.f, 18.f};
        t.radius = 4.f;
        t.spawns = {{EnemyType::Skeleton, {-3.f, 0.f, 22.f}},
                    {EnemyType::Skeleton, {3.f, 0.f, 22.f}}};
        data.spawnTriggers.push_back(t);
    }

    addShieldItem(data, "s1_1_shield_silver0", {0.f, 0.f, 6.f}, ShieldType::Silver);

    addGripItem(data, "s1_1_fire_grip0", {5.f, 0.5f, -3.f}, GripType::Fire);
    addGripItem(data, "s1_1_fire_grip1", {-5.f, 0.5f, -3.f}, GripType::Fire);

    addDecor(data, "s1_1_grave_0", {-15.f, 0.f, -10.f}, "grave_spirit");
    addDecor(data, "s1_1_rock_0", {18.f, 0.f, 0.f}, "rock_1");
    addDecor(data, "s1_1_tree_0", {15.f, 0.f, -10.f}, "tree_noLeaves_2");

    addWarpPad(data, "s1_1_return", {0.f, 0.f, -25.f}, StageId::Terminal, 1.5f);
}

// ===========================================================================
// Stage 1-2: ゲート 2 枚を 0.5m 離す (中央通路 0.5m、 通り抜け不可)
// ===========================================================================
void buildStage1_2(WorldData& data) {
    addPlatform(data, "s1_2_spawn", {0.f, -0.2f, 0.f}, {10.f, 0.2f, 8.f}, "wood_floor");

    addStaircase(data, "s1_2_stair_", 7, {0.f, 0.f, 6.f}, {0.f, 0.f, 2.f}, 0.5f, {4.f, 0.4f, 1.6f},
                 "wood_floor");

    addPlatform(data, "s1_2_top_platform", {0.f, 3.3f, 22.f}, {6.f, 0.4f, 6.f}, "wood_floor");

    // ─── 開き戸 (Rotate) V 字両開きゲート ─────────────────
    // 板中心位置を ±1.0 に分離 (前回 ±0.75 から離した)。
    // 板幅 1.5m → 内側端は左板 X=-0.25、 右板 X=+0.25、 中央に 0.5m の隙間。
    // pos.y = 3.5 (top_platform 上面 Y=3.5 に接地)、 高さ 3m。
    addRotateGate(data, "s1_2_gate_left", {-1.0f, 3.5f, 18.5f}, {1.5f, 3.f, 0.3f},
                  /*hingeOffsetLocal=*/{-0.75f, 0.f, 0.f},
                  /*openYawDelta=*/-75.f,
                  /*duration=*/1.0f, /*interactRange=*/3.5f,
                  /*groupId=*/1, /*closedYaw=*/0.f, "stone_wall");
    addRotateGate(data, "s1_2_gate_right", {1.0f, 3.5f, 18.5f}, {1.5f, 3.f, 0.3f},
                  /*hingeOffsetLocal=*/{0.75f, 0.f, 0.f},
                  /*openYawDelta=*/+75.f,
                  /*duration=*/1.0f, /*interactRange=*/3.5f,
                  /*groupId=*/1, /*closedYaw=*/0.f, "stone_wall");

    addPlatform(data, "s1_2_south_island_0", {0.f, -0.2f, -10.f}, {4.f, 0.2f, 4.f}, "wood_floor");
    addPlatform(data, "s1_2_south_island_1", {0.f, -0.2f, -18.f}, {6.f, 0.2f, 4.f}, "wood_floor");

    addPlatform(data, "s1_2_bridge_e", {7.f, -0.2f, 0.f}, {6.f, 0.2f, 1.5f}, "wood_floor");
    addPlatform(data, "s1_2_bridge_w", {-7.f, -0.2f, 0.f}, {6.f, 0.2f, 1.5f}, "wood_floor");

    addPlatform(data, "s1_2_island_e", {16.f, -0.2f, 0.f}, {4.f, 0.2f, 4.f}, "wood_floor");
    addPlatform(data, "s1_2_island_w", {-16.f, -0.2f, 0.f}, {4.f, 0.2f, 4.f}, "wood_floor");

    SpawnSystem::createSkeleton(data.world, "s1_2_sk0", {2.f, 3.5f, 24.f}, &data.enemies, 0.f,
                                &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "s1_2_sk1", {-2.f, 3.5f, 25.f}, &data.enemies, 0.f,
                                &data.vulkan);

    auto addGhost = [&](const char* name, glm::vec3 pos) {
        data.enemies.push_back(data.world.entity(name)
                                   .set<CTransform>({pos, 0.f, {0.5f, 0.8f, 0.5f}})
                                   .set<CEnemyAI>({EnemyState::Chase, 3.0f})
                                   .set<CHealth>({1, 1})
                                   .add<EnemyTag>()
                                   .add<GhostTag>());
    };
    addGhost("s1_2_gh0", {16.f, 1.5f, 0.f});
    addGhost("s1_2_gh1", {-16.f, 1.5f, 0.f});

    {
        SpawnTrigger t;
        t.center = {0.f, 3.3f, 22.f};
        t.radius = 5.f;
        t.spawns = {{EnemyType::Ghost, {0.f, 5.0f, 24.f}},
                    {EnemyType::Ghost, {-3.f, 5.0f, 22.f}},
                    {EnemyType::Ghost, {3.f, 5.0f, 22.f}}};
        data.spawnTriggers.push_back(t);
    }

    addShieldItem(data, "s1_2_shield_gold0", {0.f, 3.7f, 20.f}, ShieldType::Gold);

    addGripItem(data, "s1_2_fire_grip0", {16.f, 0.5f, 0.f}, GripType::Fire);
    addGripItem(data, "s1_2_fire_grip1", {-16.f, 0.5f, 0.f}, GripType::Fire);

    addDecor(data, "s1_2_grave_0", {3.f, 3.7f, 25.f}, "grave_1");
    addDecor(data, "s1_2_rock_0", {16.f, -0.2f, 3.5f}, "rock_1");
    addDecor(data, "s1_2_tree_0", {-16.f, -0.2f, 3.5f}, "tree_noLeaves_1");

    addWarpPad(data, "s1_2_return", {0.f, 3.7f, 22.f}, StageId::Terminal, 1.5f);
}

void buildStage1_3(WorldData& data) {
    addTerrain(data, polygon::rectangle({-12.f, 0.f}, {16.f, 16.f}), 5.0f, terrain_profile::flat,
               "grass_field", /*cellSize=*/1.0f);

    addPlatform(data, "s1_3_a_wall_east", {-4.5f, 0.f, 0.f}, {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_a_wall_west", {-19.5f, 0.f, 0.f}, {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_a_wall_north", {-12.f, 0.f, 8.5f}, {16.f, 5.f, 1.f}, "stone_wall");
    addPlatform(data, "s1_3_a_wall_south", {-12.f, 0.f, -8.5f}, {16.f, 5.f, 1.f}, "stone_wall");

    addPlatform(data, "s1_3_a_overhang", {-3.5f, 4.f, 0.f}, {2.f, 1.f, 16.f}, "stone_wall");

    addTerrain(data, polygon::rectangle({12.f, 0.f}, {16.f, 16.f}), 5.0f, terrain_profile::flat,
               "grass_field", /*cellSize=*/1.0f);

    addPlatform(data, "s1_3_b_wall_west", {4.5f, 0.f, 0.f}, {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_b_wall_east", {19.5f, 0.f, 0.f}, {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_b_wall_north", {12.f, 0.f, 8.5f}, {16.f, 5.f, 1.f}, "stone_wall");
    addPlatform(data, "s1_3_b_wall_south", {12.f, 0.f, -8.5f}, {16.f, 5.f, 1.f}, "stone_wall");

    addPlatform(data, "s1_3_b_overhang", {3.5f, 4.f, 0.f}, {2.f, 1.f, 16.f}, "stone_wall");

    addPlatform(data, "s1_3_center_island", {0.f, 3.f, 0.f}, {1.5f, 0.5f, 1.5f}, "wood_floor");

    addMovingPlatform(data, "s1_3_mover_pingpong", {-6.f, 4.f, 4.f}, {2.f, 0.4f, 2.f},
                      CMovingPlatform::Pattern::PingPongLinear, glm::vec3{1.f, 0.f, 0.f}, 5.f,
                      1.25f, 0.f, "wood_floor");

    addMovingPlatform(data, "s1_3_mover_vertical", {6.f, 4.f, -4.f}, {2.f, 0.4f, 2.f},
                      CMovingPlatform::Pattern::Vertical, glm::vec3{0.f, 1.f, 0.f}, 1.5f, 1.57f,
                      1.57f, "wood_floor");

    addMovingPlatform(data, "s1_3_mover_pendulum", {0.f, 10.f, 6.f}, {1.8f, 0.3f, 1.8f},
                      CMovingPlatform::Pattern::Pendulum, glm::vec3{1.f, 0.f, 0.f}, 4.f, 1.2f, 0.f,
                      "wood_floor");

    addMovingPlatform(data, "s1_3_mover_orbit", {0.f, 6.f, -6.f}, {1.8f, 0.3f, 1.8f},
                      CMovingPlatform::Pattern::OrbitVertical, glm::vec3{1.f, 0.f, 0.f}, 2.f, 1.0f,
                      0.f, "wood_floor");

    addWarpPad(data, "s1_3_return", {12.f, 5.f, 0.f}, StageId::Terminal, 1.5f);

    addDecor(data, "s1_3_b_tree", {15.f, 5.f, 5.f}, "tree_noLeaves_2");

    addGripItem(data, "s1_3_fire_grip0", {-12.f, 8.f, -4.f}, GripType::Fire);
    addGripItem(data, "s1_3_light_grip0", {12.f, 8.f, -4.f}, GripType::Light);
}

void buildStage1_4(WorldData& data) {
    const char* kMat = "grass_field";
    constexpr float kBaseY = 5.0f;

    addTerrain(data, polygon::ellipse({-80.f, -90.f}, 22.f, 22.f, 24), kBaseY,
               terrain_profile::rollingHills, kMat, /*cellSize=*/1.5f);

    addTerrain(data, polygon::rectangle({-30.f, -20.f}, {18.f, 60.f}), kBaseY,
               terrain_profile::rollingHills, kMat, /*cellSize=*/1.5f);

    addTerrain(data, polygon::circle({-20.f, 30.f}, 40.f, 32), kBaseY,
               terrain_profile::rollingHills, kMat, /*cellSize=*/1.5f);

    addTerrain(data, polygon::ellipse({80.f, 80.f}, 50.f, 20.f, 32), kBaseY,
               terrain_profile::rollingHills, kMat, /*cellSize=*/1.5f);

    addWarpPad(data, "s1_4_warp_end", {120.f, kBaseY + 1.0f, 90.f}, StageId::Terminal, 2.0f);

    addDecor(data, "s1_4_start_grave", {-80.f, kBaseY + 1.0f, -100.f}, "grave_spirit");
    addDecor(data, "s1_4_end_tree", {115.f, kBaseY + 1.0f, 85.f}, "tree_noLeaves_2");
}

const std::vector<StageDef>& stageList() {
    static const std::vector<StageDef> kList = [] {
        std::vector<StageDef> v;
        v.push_back(StageDef{StageId::Terminal, "Terminal", {0.f, 0.f, 0.f}, &buildTerminal});
        v.push_back(StageDef{StageId::Stage1_1, "Stage 1-1", {0.f, 0.f, 0.f}, &buildStage1_1});
        v.push_back(StageDef{StageId::Stage1_2, "Stage 1-2", {0.f, 0.f, 0.f}, &buildStage1_2});
        v.push_back(StageDef{StageId::Stage1_3, "Stage 1-3", {-12.f, 5.f, 0.f}, &buildStage1_3});
        v.push_back(StageDef{StageId::Stage1_4, "Stage 1-4", {-80.f, 6.5f, -90.f}, &buildStage1_4});
        return v;
    }();
    return kList;
}

}  // namespace

namespace stage_registry {

const std::vector<StageDef>& all() { return stageList(); }

const StageDef& get(StageId id) {
    for (const auto& def : stageList()) {
        if (def.id == id) return def;
    }
    std::cerr << "[StageRegistry] WARNING: stage not found (id=" << static_cast<int>(id)
              << "), falling back to Terminal\n";
    return stageList()[0];
}

std::vector<const StageDef*> realStages() {
    std::vector<const StageDef*> v;
    for (const auto& def : stageList()) {
        if (def.id != StageId::Terminal) {
            v.push_back(&def);
        }
    }
    return v;
}

}  // namespace stage_registry

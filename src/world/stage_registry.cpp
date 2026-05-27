// =============================================================================
// stage_registry.cpp — + addWater + Stage 1-1 くぼみに水配置
// =============================================================================
#include "world/stage_def.h"
#include <cassert>
#include <iostream>
#include <utility>
#include <glm/glm.hpp>
#include "core/chest.h"
#include "core/chest_preset.h"
#include "core/components.h"
#include "core/game_state.h"
#include "core/gate.h"
#include "core/grave.h"
#include "core/grave_fx.h"
#include "core/grip.h"
#include "core/key.h"
#include "core/money.h"
#include "core/obstacle.h"
#include "core/potion.h"
#include "core/spawn_trigger.h"
#include "core/water.h"
#include "renderer/material.h"
#include "renderer/model.h"
#include "renderer/model_scale_registry.h"
#include "renderer/resource_factory.h"
#include "renderer/terrain_mesh.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_renderer.h"
#include "renderer/water_mesh.h"
#include "systems/spawn_system.h"
#include "world/polygon_helpers.h"
#include "world/terrain_profiles.h"
namespace {
flecs::entity addPlatform(WorldData& data, const char* name, glm::vec3 pos, glm::vec3 scale,
                          const char* materialName = nullptr) {
    flecs::entity e = data.world.entity(name)
                          .set<CTransform>({pos, 0.f, scale})
                          .add<PlatformTag>();
    if (materialName) {
        const Material* mat = data.vulkan.assets().getMaterial(materialName);
        if (mat) {
            e.set<CMaterialRef>({mat});
        } else {
            std::cerr << "[StageRegistry] material not found: " << materialName << "\n";
        }
    }
    data.platforms.push_back(e);
    return e;
}
flecs::entity addMovingPlatform(WorldData& data, const char* name, glm::vec3 pos, glm::vec3 scale,
                                 CMovingPlatform::Pattern pattern,
                                 glm::vec3 axis, float amplitude, float angularSpeed,
                                 float initialPhase = 0.f,
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
                            glm::vec3 openOffset, float duration = 1.0f,
                            float interactRange = 3.0f, int groupId = 0,
                            bool requiresKey = false, KeyType requiredKey = KeyType::Gold,
                            const char* materialName = nullptr) {
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
    g.requiresKey = requiresKey;
    g.requiredKey = requiredKey;
    e.set<CGate>(g);
    e.add<GateTag>();
    data.gates.push_back(e);
    return e;
}
flecs::entity addRotateGate(WorldData& data, const char* name, glm::vec3 pos, glm::vec3 scale,
                             glm::vec3 hingeOffsetLocal, float openYawDelta,
                             float duration = 1.0f, float interactRange = 3.0f,
                             int groupId = 0, float closedYaw = 0.f,
                             bool requiresKey = false, KeyType requiredKey = KeyType::Gold,
                             const char* materialName = nullptr) {
    flecs::entity e = data.world.entity(name)
                          .set<CTransform>({pos, closedYaw, scale})
                          .add<PlatformTag>();
    if (materialName) {
        const Material* mat = data.vulkan.assets().getMaterial(materialName);
        if (mat) e.set<CMaterialRef>({mat});
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
    g.requiresKey = requiresKey;
    g.requiredKey = requiredKey;
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
    if (materialName) mat = data.vulkan.assets().getMaterial(materialName);
    auto mesh = std::make_unique<TerrainMesh>();
    // PART3c: world terrain also lives in the shared GeometryBuffer (full static
    // unification; multi-block is fine, the draw loop binds per block).
    mesh->init(&data.vulkan.context(), &data.vulkan.resources(), polygonXZ, baseY,
                heightFunc, cellSize, uvScale, mat, &data.vulkan.assets().geometry());
    data.terrains.add(std::move(mesh));
}

// ─── 水面追加ヘルパー ────
// WaterMesh を生成して WorldWater に追加し、 CWater コンポーネントを持つ entity を
// flecs world に登録する (scene_builder が拾う)。
// center.y が水面の Y 座標。
flecs::entity addWater(WorldData& data, const char* name, glm::vec3 center, glm::vec2 sizeXZ,
                       int resolution = 32, WaterDrawParams params = {}) {
    auto mesh = std::make_unique<WaterMesh>();
    mesh->init(&data.vulkan.context(), &data.vulkan.resources(), center, sizeXZ, resolution);

    CWater w{};
    w.center = center;
    w.sizeXZ = sizeXZ;
    w.mesh = mesh.get();  // WorldWater が所有、 ここはポインタだけ
    w.drawParams = params;

    flecs::entity e = data.world.entity(name)
                          .set<CWater>(w)
                          .add<WaterTag>();

    data.waters.add(std::move(mesh));
    return e;
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
    if (!model || model->empty()) return;
    const glm::vec3 scale = model_scale::get(modelName, model_scale::Context::Default);
    flecs::entity e = data.world.entity(name)
                          .set<CTransform>({pos, yaw, scale})
                          .set<CStaticModelRef>({model})
                          .set<CObstacle>({model->localAABB()})
                          .add<ObstacleTag>();
    data.decorations.push_back(e);
    data.obstacles.push_back(e);
}
flecs::entity addGrave(WorldData& data, const char* name, glm::vec3 pos, float yaw = 0.f) {
    const char* modelName = "grave_spirit";
    const Model* model = data.vulkan.assets().getModel(modelName);
    if (!model || model->empty()) {
        std::cerr << "[StageRegistry] grave_spirit model not found\n";
        return flecs::entity::null();
    }
    const glm::vec3 scale = model_scale::get(modelName, model_scale::Context::Default);
    flecs::entity e = data.world.entity(name)
                          .set<CTransform>({pos, yaw, scale})
                          .set<CStaticModelRef>({model})
                          .set<CObstacle>({model->localAABB()})
                          .set<CGrave>({})
                          .add<GraveTag>()
                          .add<ObstacleTag>();
    data.graves.push_back(e);
    data.obstacles.push_back(e);
    grave_fx::attachEmitter(e);
    return e;
}
void addShieldItem(WorldData& data, const char* name, glm::vec3 pos, ShieldType type) {
    const char* modelName = (type == ShieldType::Iron)   ? "shield_iron" :
                            (type == ShieldType::Silver) ? "shield_silver" : "shield_gold";
    const Model* model = data.vulkan.assets().getModel(modelName);
    glm::vec3 itemPos = pos;
    itemPos.y = 0.5f;
    const glm::vec3 scale = model_scale::get(modelName, model_scale::Context::Default);
    auto e = data.world.entity(name)
                 .set<CTransform>({itemPos, 0.f, scale})
                 .set<CPickup>({type})
                 .set<CSpin>({90.f})
                 .add<ShieldItemTag>();
    if (model && !model->empty()) e.set<CStaticModelRef>({model});
    data.shieldItems.push_back(e);
}
void addArmorItem(WorldData& data, const char* name, glm::vec3 pos) {
    const char* modelName = "armor";
    const Model* model = data.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty())
        scale = model_scale::get(modelName, model_scale::Context::Default);
    auto e = data.world.entity(name)
                 .set<CTransform>({pos, 0.f, scale})
                 .set<CSpin>({90.f})
                 .add<ArmorItemTag>();
    if (model && !model->empty()) e.set<CStaticModelRef>({model});
    data.armorItems.push_back(e);
}
void addGripItem(WorldData& data, const char* name, glm::vec3 pos, GripType type) {
    const GripDef& gdef = grip::def(type);
    const Model* model = data.vulkan.assets().getModel(gdef.modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty())
        scale = model_scale::get(gdef.modelName, model_scale::Context::Default);
    auto e = data.world.entity(name)
                 .set<CTransform>({pos, 0.f, scale})
                 .set<CGripPickup>({type})
                 .set<CSpin>({90.f})
                 .add<GripItemTag>();
    if (model && !model->empty()) e.set<CStaticModelRef>({model});
    grip::attachItemEmitter(e, type);
    data.gripItems.push_back(e);
}
void addKeyItem(WorldData& data, const char* name, glm::vec3 pos, KeyType type) {
    const char* modelName = nullptr;
    switch (type) {
        case KeyType::Gold:   modelName = "gold_key"; break;
        case KeyType::Silver: modelName = "silver_key"; break;
    }
    const Model* model = data.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty())
        scale = model_scale::get(modelName, model_scale::Context::Default);
    auto e = data.world.entity(name)
                 .set<CTransform>({pos, 0.f, scale})
                 .set<CKeyPickup>({type})
                 .set<CSpin>({90.f})
                 .add<KeyItemTag>();
    if (model && !model->empty()) e.set<CStaticModelRef>({model});
    data.keyItems.push_back(e);
}
void addMoneyItem(WorldData& data, const char* name, glm::vec3 pos, MoneyType type) {
    const char* modelName = moneyModelName(type);
    const Model* model = data.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty())
        scale = model_scale::get(modelName, model_scale::Context::Default);
    auto e = data.world.entity(name)
                 .set<CTransform>({pos, 0.f, scale})
                 .set<CMoneyPickup>({type})
                 .set<CSpin>({90.f})
                 .add<MoneyItemTag>();
    if (model && !model->empty()) e.set<CStaticModelRef>({model});
    data.moneyItems.push_back(e);
}
void addPotionItem(WorldData& data, const char* name, glm::vec3 pos, PotionType type) {
    const char* modelName = potion::modelName(type);
    const Model* model = data.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty())
        scale = model_scale::get(modelName, model_scale::Context::Default);
    auto e = data.world.entity(name)
                 .set<CTransform>({pos, 0.f, scale})
                 .set<CPotionPickup>({type})
                 .set<CSpin>({90.f})
                 .add<PotionItemTag>();
    if (model && !model->empty()) e.set<CStaticModelRef>({model});
    data.potionItems.push_back(e);
}
flecs::entity addChest(WorldData& data, const char* name, glm::vec3 pos,
                        ChestPresetId presetId,
                        bool requiresKey = false,
                        KeyType requiredKey = KeyType::Silver,
                        float yaw = 0.f) {
    const char* modelName = requiresKey ? "locked_chest" : "unlocked_chest";
    const Model* model = data.vulkan.assets().getModel(modelName);
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    if (model && !model->empty())
        scale = model_scale::get(modelName, model_scale::Context::Default);
    auto e = data.world.entity(name)
                 .set<CTransform>({pos, yaw, scale})
                 .add<ChestTag>();
    CChest c{};
    c.state = CChest::State::Closed;
    c.requiresKey = requiresKey;
    c.requiredKey = requiredKey;
    c.contents = chest_preset::contents(presetId);
    e.set<CChest>(c);
    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
        e.set<CObstacle>({model->localAABB()});
        e.add<ObstacleTag>();
        data.obstacles.push_back(e);
    }
    data.chests.push_back(e);
    return e;
}
// ===========================================================================
// Terminal
// ===========================================================================
void buildTerminal(WorldData& data) {
    addTerrain(data, polygon::rectangle({0.f, 0.f}, {50.f, 50.f}), 0.0f,
                terrain_profile::flat, "grass_field");
    addPlatform(data, "terminal_wall_n", {0.f, 0.f,  18.f}, {30.f, 3.f, 0.5f}, "stone_wall");
    addPlatform(data, "terminal_wall_s", {0.f, 0.f, -18.f}, {30.f, 3.f, 0.5f}, "stone_wall");
    addPlatform(data, "terminal_wall_e", { 18.f, 0.f, 0.f}, {0.5f, 3.f, 30.f}, "stone_wall");
    addPlatform(data, "terminal_wall_w", {-18.f, 0.f, 0.f}, {0.5f, 3.f, 30.f}, "stone_wall");
    const auto reals = stage_registry::realStages();
    const int n = static_cast<int>(reals.size());
    constexpr float kRadius = 8.f;
    for (int i = 0; i < n; ++i) {
        const float angle = (3.14159265f * 2.f) * static_cast<float>(i) /
                             static_cast<float>(n > 0 ? n : 1);
        const glm::vec3 pos{kRadius * std::cos(angle), 0.f, kRadius * std::sin(angle)};
        const std::string padName = "warp_to_" + std::to_string(static_cast<int>(reals[i]->id));
        addWarpPad(data, padName.c_str(), pos, reals[i]->id, 1.5f);
    }
    SpawnSystem::createSkeleton(data.world, "term_demo_sk0", {0.f, 0.f, 12.f},
                                  &data.enemies, 0.f, &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "term_demo_sk1", {-6.f, 0.f, -10.f},
                                  &data.enemies, 0.f, &data.vulkan);
    addShieldItem(data, "term_shield_iron0", {4.f, 0.f, -2.f}, ShieldType::Iron);
    addGripItem(data, "term_fire_grip0", {-4.f, 0.5f, 4.f}, GripType::Fire);
    addGripItem(data, "term_fire_grip1", {6.f,  0.5f, 6.f}, GripType::Fire);
    addArmorItem(data, "term_armor0", {2.f, 0.5f, -4.f});
    addMoneyItem(data, "term_coin0",     {-2.f, 0.5f, -4.f}, MoneyType::Coin);
    addMoneyItem(data, "term_coin_bag0", {-4.f, 0.5f, -6.f}, MoneyType::CoinBag);
    addMoneyItem(data, "term_diamond0",  {-6.f, 0.5f, -8.f}, MoneyType::Diamond);
    addPotionItem(data, "term_potion_s0", {0.f, 0.5f, -2.f}, PotionType::Small);
    addPotionItem(data, "term_potion_s1", {-2.f, 0.5f, -2.f}, PotionType::Small);
    addChest(data, "term_chest_coins5",    {-6.f, 0.f, 8.f}, ChestPresetId::Coins5);
    addChest(data, "term_chest_diaCoin",   {-2.f, 0.f, 8.f}, ChestPresetId::DiamondAndCoin);
    addChest(data, "term_chest_armor",     { 2.f, 0.f, 8.f}, ChestPresetId::Armor1);
    addChest(data, "term_chest_fireGrip",  { 6.f, 0.f, 8.f}, ChestPresetId::FireGrip1);
    addChest(data, "term_chest_potion",    {10.f, 0.f, 8.f}, ChestPresetId::PotionS1);
    addChest(data, "term_chest_locked_armor",    {14.f, 0.f, 8.f}, ChestPresetId::Armor1,
              /*requiresKey=*/true, KeyType::Silver);
    addChest(data, "term_chest_locked_fireGrip", {16.f, 0.f, 8.f}, ChestPresetId::FireGrip1,
              /*requiresKey=*/true, KeyType::Silver);
    addKeyItem(data, "term_silver_key_demo0", {10.f, 0.5f, 5.f}, KeyType::Silver);
    addKeyItem(data, "term_silver_key_demo1", {12.f, 0.5f, 5.f}, KeyType::Silver);
    addDecor(data, "term_grave_normal", {-12.f, 0.f, -8.f}, "grave_1");
    addGrave(data, "term_grave_spirit_0", {-10.f, 0.f, -4.f});
    addGrave(data, "term_grave_spirit_1", { -8.f, 0.f, -4.f});
    addGrave(data, "term_grave_spirit_2", { -6.f, 0.f, -4.f});
    addDecor(data, "term_rock_0",  {12.f, 0.f, -8.f},  "rock_1");
    addDecor(data, "term_tree_0",  {0.f, 0.f, -14.f},  "tree_noLeaves_1");
}
void buildStage1_1(WorldData& data) {
    addTerrain(data, polygon::rectangle({0.f, 0.f}, {100.f, 100.f}), 0.0f,
                terrain_profile::stage1_1_terrain, "grass_field", /*cellSize=*/1.5f);

    // ─── 水面 (くぼみ中心 (-12, ?, -12)、 σ=7m、 最大深 3m) ────
    // 水面 Y = -0.5 (深さ最大オフセット -3 + 余裕)、 サイズ 16m × 16m (= 2σ + α)
    addWater(data, "s1_1_pool", {-12.f, -0.5f, -12.f}, {16.f, 16.f}, /*resolution=*/32);

    SpawnSystem::createSkeleton(data.world, "s1_1_sk0", {10.f, 0.f, 8.f},
                                  &data.enemies, 0.f, &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "s1_1_sk1", {15.f, 0.f, 5.f},
                                  &data.enemies, 0.f, &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "s1_1_sk2", {12.f, 0.f, 12.f},
                                  &data.enemies, 0.f, &data.vulkan);
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
    addGripItem(data, "s1_1_fire_grip0", {5.f,  0.5f, -3.f}, GripType::Fire);
    addGripItem(data, "s1_1_fire_grip1", {-5.f, 0.5f, -3.f}, GripType::Fire);
    addGrave(data, "s1_1_grave_0", {-15.f, 0.f, -10.f});
    addDecor(data, "s1_1_rock_0",  {18.f, 0.f, 0.f},    "rock_1");
    addDecor(data, "s1_1_tree_0",  {15.f, 0.f, -10.f},  "tree_noLeaves_2");
    addWarpPad(data, "s1_1_return", {0.f, 0.f, -25.f}, StageId::Terminal, 1.5f);
}
void buildStage1_2(WorldData& data) {
    addPlatform(data, "s1_2_spawn", {0.f, -0.2f, 0.f}, {10.f, 0.2f, 8.f}, "wood_floor");
    addStaircase(data, "s1_2_stair_", 7, {0.f, 0.f, 6.f}, {0.f, 0.f, 2.f}, 0.5f,
                  {4.f, 0.4f, 1.6f}, "wood_floor");
    addPlatform(data, "s1_2_top_platform", {0.f, 3.3f, 22.f}, {6.f, 0.4f, 6.f}, "wood_floor");
    addPlatform(data, "s1_2_deep_platform", {0.f, 3.3f, 32.f}, {6.f, 0.4f, 6.f}, "wood_floor");
    addRotateGate(data, "s1_2_gate_gold_left",  {-1.0f, 3.5f, 18.5f}, {1.5f, 3.f, 0.3f},
                  {-0.75f, 0.f, 0.f}, -75.f, 1.0f, 3.5f, 1, 0.f,
                  true, KeyType::Gold, "stone_wall");
    addRotateGate(data, "s1_2_gate_gold_right", { 1.0f, 3.5f, 18.5f}, {1.5f, 3.f, 0.3f},
                  { 0.75f, 0.f, 0.f}, +75.f, 1.0f, 3.5f, 1, 0.f,
                  true, KeyType::Gold, "stone_wall");
    addSlideGate(data, "s1_2_gate_silver", {0.f, 3.5f, 27.f}, {4.f, 3.f, 0.3f},
                  {0.f, 3.5f, 0.f}, 1.5f, 3.5f, 2, true, KeyType::Silver, "stone_wall");
    addPlatform(data, "s1_2_south_island_0", {0.f, -0.2f, -10.f}, {4.f, 0.2f, 4.f}, "wood_floor");
    addPlatform(data, "s1_2_south_island_1", {0.f, -0.2f, -18.f}, {6.f, 0.2f, 4.f}, "wood_floor");
    addPlatform(data, "s1_2_bridge_e", {7.f, -0.2f, 0.f}, {6.f, 0.2f, 1.5f}, "wood_floor");
    addPlatform(data, "s1_2_bridge_w", {-7.f, -0.2f, 0.f}, {6.f, 0.2f, 1.5f}, "wood_floor");
    addPlatform(data, "s1_2_island_e", {16.f, -0.2f, 0.f}, {4.f, 0.2f, 4.f}, "wood_floor");
    addPlatform(data, "s1_2_island_w", {-16.f, -0.2f, 0.f}, {4.f, 0.2f, 4.f}, "wood_floor");
    SpawnSystem::createSkeleton(data.world, "s1_2_sk0", {2.f, 3.5f, 24.f},
                                  &data.enemies, 0.f, &data.vulkan);
    SpawnSystem::createSkeleton(data.world, "s1_2_sk1", {-2.f, 3.5f, 25.f},
                                  &data.enemies, 0.f, &data.vulkan);
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
    addGripItem(data, "s1_2_fire_grip0", {16.f,  0.5f, 0.f}, GripType::Fire);
    addGripItem(data, "s1_2_fire_grip1", {-16.f, 0.5f, 0.f}, GripType::Fire);
    addKeyItem(data, "s1_2_gold_key",   {0.f, 0.5f, 3.f},  KeyType::Gold);
    addKeyItem(data, "s1_2_silver_key", {3.f, 3.7f, 22.f}, KeyType::Silver);
    addDecor(data, "s1_2_grave_0", {3.f, 3.7f, 25.f},   "grave_1");
    addDecor(data, "s1_2_rock_0",  {16.f, -0.2f, 3.5f}, "rock_1");
    addDecor(data, "s1_2_tree_0",  {-16.f, -0.2f, 3.5f}, "tree_noLeaves_1");
    addWarpPad(data, "s1_2_return", {0.f, 3.7f, 32.f}, StageId::Terminal, 1.5f);
}
void buildStage1_3(WorldData& data) {
    addTerrain(data, polygon::rectangle({-12.f, 0.f}, {16.f, 16.f}), 5.0f,
                terrain_profile::flat, "grass_field", 1.0f);
    addPlatform(data, "s1_3_a_wall_east",   {-4.5f, 0.f, 0.f},  {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_a_wall_west",   {-19.5f, 0.f, 0.f}, {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_a_wall_north",  {-12.f, 0.f, 8.5f}, {16.f, 5.f, 1.f}, "stone_wall");
    addPlatform(data, "s1_3_a_wall_south",  {-12.f, 0.f, -8.5f},{16.f, 5.f, 1.f}, "stone_wall");
    addPlatform(data, "s1_3_a_overhang", {-3.5f, 4.f, 0.f}, {2.f, 1.f, 16.f}, "stone_wall");
    addTerrain(data, polygon::rectangle({12.f, 0.f}, {16.f, 16.f}), 5.0f,
                terrain_profile::flat, "grass_field", 1.0f);
    addPlatform(data, "s1_3_b_wall_west",   {4.5f, 0.f, 0.f},   {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_b_wall_east",   {19.5f, 0.f, 0.f},  {1.f, 5.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_b_wall_north",  {12.f, 0.f, 8.5f},  {16.f, 5.f, 1.f}, "stone_wall");
    addPlatform(data, "s1_3_b_wall_south",  {12.f, 0.f, -8.5f}, {16.f, 5.f, 1.f}, "stone_wall");
    addPlatform(data, "s1_3_b_overhang", {3.5f, 4.f, 0.f}, {2.f, 1.f, 16.f}, "stone_wall");
    addPlatform(data, "s1_3_center_island", {0.f, 3.f, 0.f}, {1.5f, 0.5f, 1.5f}, "wood_floor");
    addMovingPlatform(data, "s1_3_mover_pingpong", {-6.f, 4.f, 4.f}, {2.f, 0.4f, 2.f},
                      CMovingPlatform::Pattern::PingPongLinear,
                      glm::vec3{1.f, 0.f, 0.f}, 5.f, 1.25f, 0.f, "wood_floor");
    addMovingPlatform(data, "s1_3_mover_vertical", {6.f, 4.f, -4.f}, {2.f, 0.4f, 2.f},
                      CMovingPlatform::Pattern::Vertical,
                      glm::vec3{0.f, 1.f, 0.f}, 1.5f, 1.57f, 1.57f, "wood_floor");
    addMovingPlatform(data, "s1_3_mover_pendulum", {0.f, 10.f, 6.f}, {1.8f, 0.3f, 1.8f},
                      CMovingPlatform::Pattern::Pendulum,
                      glm::vec3{1.f, 0.f, 0.f}, 4.f, 1.2f, 0.f, "wood_floor");
    addMovingPlatform(data, "s1_3_mover_orbit", {0.f, 6.f, -6.f}, {1.8f, 0.3f, 1.8f},
                      CMovingPlatform::Pattern::OrbitVertical,
                      glm::vec3{1.f, 0.f, 0.f}, 2.f, 1.0f, 0.f, "wood_floor");
    addWarpPad(data, "s1_3_return", {12.f, 5.f, 0.f}, StageId::Terminal, 1.5f);
    addDecor(data, "s1_3_b_tree", {15.f, 5.f, 5.f}, "tree_noLeaves_2");
    addGripItem(data, "s1_3_fire_grip0",  {-12.f, 8.f, -4.f}, GripType::Fire);
    addGripItem(data, "s1_3_light_grip0", {12.f,  8.f, -4.f}, GripType::Light);
}
void buildStage1_4(WorldData& data) {
    const char* kMat = "grass_field";
    constexpr float kBaseY = 5.0f;
    addTerrain(data, polygon::ellipse({-80.f, -90.f}, 22.f, 22.f, 24), kBaseY,
                terrain_profile::rollingHills, kMat, 1.5f);
    addTerrain(data, polygon::rectangle({-30.f, -20.f}, {18.f, 60.f}), kBaseY,
                terrain_profile::rollingHills, kMat, 1.5f);
    addTerrain(data, polygon::circle({-20.f, 30.f}, 40.f, 32), kBaseY,
                terrain_profile::rollingHills, kMat, 1.5f);
    addTerrain(data, polygon::ellipse({80.f, 80.f}, 50.f, 20.f, 32), kBaseY,
                terrain_profile::rollingHills, kMat, 1.5f);
    addWarpPad(data, "s1_4_warp_end", {120.f, kBaseY + 1.0f, 90.f}, StageId::Terminal, 2.0f);
    addGrave(data, "s1_4_start_grave", {-80.f, kBaseY + 1.0f, -100.f});
    addDecor(data, "s1_4_end_tree",    {115.f, kBaseY + 1.0f, 85.f},   "tree_noLeaves_2");
}
const std::vector<StageDef>& stageList() {
    static const std::vector<StageDef> kList = [] {
        std::vector<StageDef> v;
        v.push_back(StageDef{StageId::Terminal, "Terminal", {0.f, 0.f, 0.f}, &buildTerminal});
        v.push_back(StageDef{StageId::Stage1_1, "Stage 1-1", {0.f, 0.f, 0.f}, &buildStage1_1});
        v.push_back(StageDef{StageId::Stage1_2, "Stage 1-2", {0.f, 0.f, 0.f}, &buildStage1_2});
        v.push_back(StageDef{StageId::Stage1_3, "Stage 1-3", {-12.f, 5.f, 0.f}, &buildStage1_3});
        v.push_back(StageDef{StageId::Stage1_4, "Stage 1-4",
                              {-80.f, 6.5f, -90.f}, &buildStage1_4});
        return v;
    }();
    return kList;
}
}  // namespace
namespace stage_registry {
const std::vector<StageDef>& all() { return stageList(); }
const StageDef& get(StageId id) {
    for (const auto& def : stageList()) if (def.id == id) return def;
    return stageList()[0];
}
std::vector<const StageDef*> realStages() {
    std::vector<const StageDef*> v;
    for (const auto& def : stageList()) if (def.id != StageId::Terminal) v.push_back(&def);
    return v;
}
}  // namespace stage_registry

// =============================================================================
// systems/chest_system.cpp — + chest open SE (Opened 時のみ)
// =============================================================================
#define NOMINMAX
#include "systems/chest_system.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "core/chest.h"
#include "core/components.h"
#include "core/game_state.h"
#include "core/grip.h"
#include "core/key.h"
#include "core/money.h"
#include "core/obstacle.h"
#include "core/potion.h"
#include "renderer/model.h"
#include "renderer/model_scale_registry.h"
#include "renderer/vulkan_renderer.h"
#include "systems/sound_manager.h"
namespace {
float frand() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }
glm::vec3 randomBurstVelocity() {
    const float angle = frand() * 6.2831853f;
    const float horizSpeed = 1.0f + frand() * 1.5f;
    const float upSpeed    = 4.0f + frand() * 2.0f;
    return glm::vec3{
        std::cos(angle) * horizSpeed,
        upSpeed,
        std::sin(angle) * horizSpeed,
    };
}
void setBurstVelocity(flecs::entity e) {
    CVelocity v{};
    const glm::vec3 burst = randomBurstVelocity();
    v.y = burst.y;
    v.xz = glm::vec2{burst.x, burst.z};
    e.set<CVelocity>(v);
}
flecs::entity spawnLootMoney(WorldData& wd, glm::vec3 origin, MoneyType type) {
    const char* modelName = moneyModelName(type);
    const Model* model = wd.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty()) {
        scale = model_scale::get(modelName, model_scale::Context::Default);
    }
    static int s_idx = 0;
    const std::string name = "chest_loot_money_" + std::to_string(s_idx++);
    auto e = wd.world.entity(name.c_str())
                 .set<CTransform>({origin, 0.f, scale})
                 .set<CMoneyPickup>({type})
                 .set<CSpin>({90.f})
                 .add<MoneyItemTag>();
    setBurstVelocity(e);
    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
    }
    wd.moneyItems.push_back(e);
    return e;
}
flecs::entity spawnLootArmor(WorldData& wd, glm::vec3 origin) {
    const char* modelName = "armor";
    const Model* model = wd.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty()) {
        scale = model_scale::get(modelName, model_scale::Context::Default);
    }
    static int s_idx = 0;
    const std::string name = "chest_loot_armor_" + std::to_string(s_idx++);
    auto e = wd.world.entity(name.c_str())
                 .set<CTransform>({origin, 0.f, scale})
                 .set<CSpin>({90.f})
                 .add<ArmorItemTag>();
    setBurstVelocity(e);
    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
    }
    wd.armorItems.push_back(e);
    return e;
}
flecs::entity spawnLootKey(WorldData& wd, glm::vec3 origin, KeyType type) {
    const char* modelName = nullptr;
    switch (type) {
        case KeyType::Gold:   modelName = "gold_key";   break;
        case KeyType::Silver: modelName = "silver_key"; break;
    }
    const Model* model = wd.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty()) {
        scale = model_scale::get(modelName, model_scale::Context::Default);
    }
    static int s_idx = 0;
    const std::string name = "chest_loot_key_" + std::to_string(s_idx++);
    auto e = wd.world.entity(name.c_str())
                 .set<CTransform>({origin, 0.f, scale})
                 .set<CKeyPickup>({type})
                 .set<CSpin>({90.f})
                 .add<KeyItemTag>();
    setBurstVelocity(e);
    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
    }
    wd.keyItems.push_back(e);
    return e;
}
flecs::entity spawnLootGrip(WorldData& wd, glm::vec3 origin, GripType type) {
    const GripDef& gdef = grip::def(type);
    const Model* model = wd.vulkan.assets().getModel(gdef.modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty()) {
        scale = model_scale::get(gdef.modelName, model_scale::Context::Default);
    }
    static int s_idx = 0;
    const std::string name = "chest_loot_grip_" + std::to_string(s_idx++);
    auto e = wd.world.entity(name.c_str())
                 .set<CTransform>({origin, 0.f, scale})
                 .set<CGripPickup>({type})
                 .set<CSpin>({90.f})
                 .add<GripItemTag>();
    setBurstVelocity(e);
    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
    }
    grip::attachItemEmitter(e, type);
    wd.gripItems.push_back(e);
    return e;
}
flecs::entity spawnLootPotion(WorldData& wd, glm::vec3 origin, PotionType type) {
    const char* modelName = potion::modelName(type);
    const Model* model = wd.vulkan.assets().getModel(modelName);
    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty()) {
        scale = model_scale::get(modelName, model_scale::Context::Default);
    }
    static int s_idx = 0;
    const std::string name = "chest_loot_potion_" + std::to_string(s_idx++);
    auto e = wd.world.entity(name.c_str())
                 .set<CTransform>({origin, 0.f, scale})
                 .set<CPotionPickup>({type})
                 .set<CSpin>({90.f})
                 .add<PotionItemTag>();
    setBurstVelocity(e);
    if (model && !model->empty()) {
        e.set<CStaticModelRef>({model});
    }
    wd.potionItems.push_back(e);
    return e;
}
void spawnOneReward(WorldData& wd, glm::vec3 origin, const ChestReward& r) {
    for (int i = 0; i < r.count; ++i) {
        switch (r.type) {
            case ChestRewardType::Coin:
                spawnLootMoney(wd, origin, MoneyType::Coin); break;
            case ChestRewardType::CoinBag:
                spawnLootMoney(wd, origin, MoneyType::CoinBag); break;
            case ChestRewardType::Diamond:
                spawnLootMoney(wd, origin, MoneyType::Diamond); break;
            case ChestRewardType::Armor:
                spawnLootArmor(wd, origin); break;
            case ChestRewardType::GoldKey:
                spawnLootKey(wd, origin, KeyType::Gold); break;
            case ChestRewardType::SilverKey:
                spawnLootKey(wd, origin, KeyType::Silver); break;
            case ChestRewardType::FireGrip:
                spawnLootGrip(wd, origin, GripType::Fire); break;
            case ChestRewardType::LightGrip:
                spawnLootGrip(wd, origin, GripType::Light); break;
            case ChestRewardType::PotionS:
                spawnLootPotion(wd, origin, PotionType::Small); break;
        }
    }
}
void spawnAllRewards(WorldData& wd, const glm::vec3& chestPos, float chestScaleY,
                      const std::vector<ChestReward>& rewards) {
    const glm::vec3 origin{
        chestPos.x,
        chestPos.y + chestScaleY * 0.6f,
        chestPos.z,
    };
    int total = 0;
    for (const auto& r : rewards) total += r.count;
    std::cout << "[Chest] spawning " << total << " reward item(s)\n";
    for (const auto& r : rewards) {
        spawnOneReward(wd, origin, r);
    }
}
}  // namespace
void ChestSystem::update(WorldData& wd, float dt) const {
    wd.world.each([&](flecs::entity, CChest& c) {
        if (c.state != CChest::State::Opening) return;
        c.progress += dt / c.duration;
        if (c.progress >= 1.f) {
            c.progress = 1.f;
            c.state = CChest::State::Open;
            c.openElapsed = 0.f;
        }
    });
    std::vector<flecs::entity> toDestruct;
    wd.world.each([&](flecs::entity e, CChest& c) {
        if (c.state != CChest::State::Open) return;
        c.openElapsed += dt;
        if (c.openElapsed >= CChest::kDestructTime) {
            toDestruct.push_back(e);
        }
    });
    for (flecs::entity e : toDestruct) {
        if (e.is_alive()) {
            std::cout << "[Chest] destructing faded chest '" << e.name().c_str() << "'\n";
            wd.chests.erase(std::remove(wd.chests.begin(), wd.chests.end(), e),
                            wd.chests.end());
            e.destruct();
        }
    }
}
flecs::entity ChestSystem::findNearestOpenableChest(WorldData& wd) const {
    if (!wd.player || !wd.player.is_alive()) return flecs::entity::null();
    if (!wd.player.has<CTransform>()) return flecs::entity::null();
    const glm::vec3& playerPos = wd.player.get<CTransform>().pos;
    flecs::entity best = flecs::entity::null();
    float bestDistSq = std::numeric_limits<float>::max();
    wd.world.each([&](flecs::entity e, const CTransform& t, const CChest& c) {
        if (!c.isOpenable()) return;
        const float dx = playerPos.x - t.pos.x;
        const float dy = playerPos.y - t.pos.y;
        const float dz = playerPos.z - t.pos.z;
        const float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > c.interactRange * c.interactRange) return;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = e;
        }
    });
    return best;
}
ChestSystem::OpenResult ChestSystem::tryOpenNearestChest(WorldData& wd, SoundManager& sound) const {
    flecs::entity chest = findNearestOpenableChest(wd);
    if (!chest || !chest.is_alive()) return OpenResult::NoChest;
    bool requiresKey;
    KeyType requiredKey;
    std::vector<ChestReward> contentsCopy;
    glm::vec3 chestPos;
    float chestScaleY;
    {
        const CChest& cRead = chest.get<CChest>();
        if (!cRead.isOpenable()) return OpenResult::NoChest;
        requiresKey = cRead.requiresKey;
        requiredKey = cRead.requiredKey;
        contentsCopy = cRead.contents;
        const CTransform& tRead = chest.get<CTransform>();
        chestPos = tRead.pos;
        chestScaleY = tRead.scale.y;
    }
    if (requiresKey) {
        const bool hasKey = wd.player && wd.player.is_alive() &&
                            wd.player.has<CKeyInventory>() &&
                            wd.player.get<CKeyInventory>().has(requiredKey);
        if (!hasKey) {
            std::cout << "[Chest] '" << chest.name().c_str() << "' needs "
                      << keyTypeName(requiredKey) << " key\n";
            return OpenResult::NeedsKey;
        }
        CKeyInventory& inv = wd.player.ensure<CKeyInventory>();
        if (inv.consume(requiredKey)) {
            std::cout << "[Key] consumed " << keyTypeName(requiredKey)
                      << " key, remaining=" << inv.count(requiredKey) << "\n";
        }
    }
    {
        CChest& c = chest.ensure<CChest>();
        c.state = CChest::State::Opening;
        c.progress = 0.f;
    }
    std::cout << "[Chest] opening '" << chest.name().c_str() << "'\n";
    if (chest.has<CObstacle>()) {
        chest.remove<CObstacle>();
    }
    if (chest.has<ObstacleTag>()) {
        chest.remove<ObstacleTag>();
    }
    wd.obstacles.erase(std::remove(wd.obstacles.begin(), wd.obstacles.end(), chest),
                       wd.obstacles.end());
    std::cout << "[Chest] obstacle collision removed for '" << chest.name().c_str() << "'\n";
    spawnAllRewards(wd, chestPos, chestScaleY, contentsCopy);

    // 開ける瞬間 SE 発火
    sound.playChestOpen();

    return OpenResult::Opened;
}

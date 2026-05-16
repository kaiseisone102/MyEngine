// =============================================================================
// systems/spirit_system.cpp — + skeletal anim 対応
// =============================================================================
// spirit.glb はスケルタルアニメ付き (5 bones, idle clip)。
// spawn 時に skeletal anim を attach する。 pool 枯渇時は static 描画にフォールバック。
//
// skinned と static が同じ entity に両立すると 2 回描画されるので、
// skinned 付与に成功した場合は CStaticModelRef を付けない。
// =============================================================================
#define NOMINMAX
#include "systems/spirit_system.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/components.h"
#include "core/game_state.h"
#include "core/spirit.h"
#include "renderer/animation.h"
#include "renderer/animator.h"
#include "renderer/asset_registry.h"
#include "renderer/model.h"
#include "renderer/model_scale_registry.h"
#include "renderer/skeleton.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/vulkan_renderer.h"

namespace {

float frand() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }

// spirit に skeletal anim を attach。 成功したら true、 pool 枯渇等で失敗したら false。
bool attachSpiritSkeletalAnim(flecs::entity e, VulkanRenderer& vulkan, const Model* model) {
    if (!model || !model->hasSkeleton()) return false;

    AssetRegistry& assets = vulkan.assets();

    // spirit 用に embedded された 'spirit' アニメを優先、 なければ 'idle' にフォールバック
    const AnimationClip* clip = assets.getAnimation("spirit");
    if (!clip) {
        std::cerr << "[Spirit] 'spirit' animation not found, falling back to 'idle'\n";
        clip = assets.getAnimation("idle");
    }
    if (!clip) {
        std::cerr << "[Spirit] no animation available for spirit, skipping skinning\n";
        return false;
    }

    CSkeletalAnim sa;
    sa.model = model;
    sa.animator.bind(&model->skeleton(), clip);
    sa.skinMatrices.assign(model->skeleton().boneCount(), glm::mat4(1.f));
    sa.playing = true;
    sa.speed = 1.f;
    sa.skinSlot = vulkan.skinBufferPool().allocate();

    if (!sa.skinSlot.valid()) {
        std::cerr << "[Spirit] SkinBufferPool full, spirit will render statically\n";
        return false;
    }

    e.set<CSkeletalAnim>(std::move(sa));
    return true;
}

}  // namespace

flecs::entity SpiritSystem::spawnSpirit(WorldData& wd, const glm::vec3& origin) {
    const char* modelName = "spirit";
    const Model* model = wd.vulkan.assets().getModel(modelName);

    glm::vec3 scale{0.5f, 0.5f, 0.5f};
    if (model && !model->empty()) {
        scale = model_scale::get(modelName, model_scale::Context::Default);
    }

    static int s_idx = 0;
    const std::string name = "spirit_" + std::to_string(s_idx++);

    const float angle = frand() * 6.2831853f;
    const float horizOffset = 0.3f + frand() * 0.4f;
    const glm::vec3 pos{
        origin.x + std::cos(angle) * horizOffset,
        origin.y + 0.5f + frand() * 0.3f,
        origin.z + std::sin(angle) * horizOffset,
    };

    auto e = wd.world.entity(name.c_str())
                 .set<CTransform>({pos, frand() * 360.f, scale})
                 .add<CSpiritPickup>()
                 .set<CSpin>({60.f + frand() * 60.f})
                 .set<CFloatingSpirit>({})
                 .add<SpiritItemTag>();

    if (model && !model->empty()) {
        // skeletal anim 付与を試みる。
        // 成功したら CStaticModelRef は付けない (= skinned のみで描画)。
        // 失敗したら CStaticModelRef でフォールバック描画 (= アニメなし)。
        const bool skinned = attachSpiritSkeletalAnim(e, wd.vulkan, model);
        if (!skinned) {
            e.set<CStaticModelRef>({model});
        }
    }

    wd.spiritItems.push_back(e);
    return e;
}

void SpiritSystem::update(WorldData& wd, float dt) const {
    std::vector<flecs::entity> toDestruct;

    for (flecs::entity item : wd.spiritItems) {
        if (!item || !item.is_alive()) continue;
        if (!item.has<CFloatingSpirit>() || !item.has<CTransform>()) continue;

        CFloatingSpirit& fs = item.ensure<CFloatingSpirit>();
        CTransform& t = item.ensure<CTransform>();

        fs.elapsed += dt;

        if (fs.phase == CFloatingSpirit::Phase::Rising &&
            fs.elapsed >= CFloatingSpirit::kSoarStartTime) {
            fs.phase = CFloatingSpirit::Phase::Soaring;
        }

        const float speed = (fs.phase == CFloatingSpirit::Phase::Soaring)
                                ? CFloatingSpirit::kSoarSpeed
                                : CFloatingSpirit::kRiseSpeed;
        t.pos.y += speed * dt;

        if (t.pos.y >= CFloatingSpirit::kDestructY) {
            toDestruct.push_back(item);
        }
    }

    for (flecs::entity e : toDestruct) {
        if (e.is_alive()) {
            wd.spiritItems.erase(std::remove(wd.spiritItems.begin(), wd.spiritItems.end(), e),
                                 wd.spiritItems.end());
            e.destruct();
        }
    }
}

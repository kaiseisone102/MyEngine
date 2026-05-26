#pragma once
// =============================================================================
// loop/gameplay_layer.h
// + Phase 1C: コンストラクタに VulkanRenderer&、 buildScene(SceneData&)
//   namespace scene { class Scene; } 削除
// =============================================================================
#include <flecs.h>

#include <memory>
#include <string>

#include "core/game_state.h"
#include "loop/layer.h"
#include "loop/layer_factory.h"
#include "scene/scene_renderer.h"
#include "systems/simulation_system.h"
#include "world/stage_id.h"
#include "world/world_builder.h"

class SceneRenderer;
class VulkanRenderer;
class SceneData;
class CAttack;
struct CTransform;

class GameplayLayer : public ILayer {
   public:
    GameplayLayer(const LayerContext& ctx, float gravity, float jumpSpeed,
                    StageId initialStage);
    ~GameplayLayer() override;

    void onEnter() override;
    void onExit() override;
    void handleEvents(const EventBus& events, LayerCommands& cmds) override;
    void update(float dt, bool isTop, const ActionState& input) override;
    void buildScene(SceneData& scene) override;
    void drawImGui() override;

    const char* name() const override { return "Gameplay"; }
    MouseCapturePolicy mouseCapturePolicy() const override {
        return MouseCapturePolicy::Locked;
    }

   private:
    void requestWarpToStage(StageId target);
    void warpToStage(StageId target);
    void pushPauseMenu(LayerCommands& cmds);
    void updateNearbyWarpPad();
    void updateNearbyGate();
    void updateNearbyChest();
    void drawAttackHitboxDebug();
    glm::vec3 getCurrentSweepWorldDirForDraw(const CAttack& atk, const CTransform& at) const;

    GameState& state_;
    SceneRenderer& renderer_;
    VulkanRenderer& vulkan_;
    ILayerFactory& factory_;
    SimulationSystem sim_;
    WorldBuilder worldBuilder_;
    float gravity_;
    float jumpSpeed_;
    StageId currentStage_;
    bool gameOverPushed_ = false;
    float deathTimer_ = 0.f;
    static constexpr float kDeathDelay = 1.5f;
    bool pendingWarp_ = false;
    StageId pendingWarpTarget_ = StageId::Terminal;
    flecs::entity nearbyWarp_ = flecs::entity::null();
    StageId nearbyWarpTarget_ = StageId::Terminal;
    std::string nearbyWarpTargetName_;
    flecs::entity nearbyGate_ = flecs::entity::null();
    flecs::entity nearbyChest_ = flecs::entity::null();
    float debugElapsedTime_ = 0.f;
    int skinFrameIndex_ = 0;
};

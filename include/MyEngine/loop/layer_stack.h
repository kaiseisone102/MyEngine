#pragma once
// =============================================================================
// layer_stack.h — Layer のスタック管理 (オーバーレイ対応)
// + Phase 1C: SceneRenderer& + VulkanRenderer& を保持
//   render() は SceneRenderer::buildSceneData + VulkanRenderer::drawFrame の
//   2 ステップに分解
// =============================================================================
#include <memory>
#include <vector>
#include "loop/layer.h"

class EventBus;
struct ActionState;
class SceneRenderer;
class VulkanRenderer;
struct GameState;

class LayerStack : public LayerCommands {
   public:
    // Phase 1C: SceneRenderer (buildSceneData 用) + VulkanRenderer (drawFrame/scene 用)
    //           + GameState (cameraPos と settings.drawDistance の参照用) を保持
    LayerStack(SceneRenderer& sceneRenderer, VulkanRenderer& vulkan, GameState& state);
    ~LayerStack() override;

    void push(std::unique_ptr<ILayer> layer);
    void requestPush(std::unique_ptr<ILayer> layer) override;
    void requestPop() override;
    void requestReplace(std::unique_ptr<ILayer> newLayer) override;
    void requestQuit() override;

    bool quitRequested() const { return quitRequested_; }
    void flushPending();

    void handleEvents(const EventBus& events);
    void update(float dt, const ActionState& input);
    void render();

    bool empty() const { return layers_.empty(); }
    size_t size() const { return layers_.size(); }
    ILayer* top() const { return layers_.empty() ? nullptr : layers_.back().get(); }

   private:
    enum class OpType { Push, Pop, Replace };
    struct PendingOp {
        OpType type;
        std::unique_ptr<ILayer> layer;
    };

    SceneRenderer& sceneRenderer_;
    VulkanRenderer& vulkan_;
    GameState& state_;
    std::vector<std::unique_ptr<ILayer>> layers_;
    std::vector<PendingOp> pending_;
    bool quitRequested_ = false;

    size_t findUpdateStartIndex() const;
    size_t findRenderStartIndex() const;
};

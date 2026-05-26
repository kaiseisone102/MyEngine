#pragma once
// =============================================================================
// layer_stack.h — Layer のスタック管理 (オーバーレイ対応)
//   render() は VulkanRenderer の SceneData を clear し、可視 Layer の buildScene
//   を積んで drawFrame に渡す 2 ステップ。
// =============================================================================
#include <memory>
#include <vector>
#include "loop/layer.h"

class EventBus;
struct ActionState;
class VulkanRenderer;
struct GameState;

class LayerStack : public LayerCommands {
   public:
    // VulkanRenderer (drawFrame / SceneData) + GameState (settings.drawDistance) を保持
    LayerStack(VulkanRenderer& vulkan, GameState& state);
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

    VulkanRenderer& vulkan_;
    GameState& state_;
    std::vector<std::unique_ptr<ILayer>> layers_;
    std::vector<PendingOp> pending_;
    bool quitRequested_ = false;

    size_t findStartIndex(bool (ILayer::*blocks)() const) const;
};

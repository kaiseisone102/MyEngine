// =============================================================================
// layer_stack.cpp — Layer のスタック管理 (オーバーレイ対応)
// =============================================================================
#include "loop/layer_stack.h"

#include <utility>

#include "core/action_state.h"
#include "core/game_state.h"
#include "renderer/vulkan_renderer.h"
#include "scene/scene_data.h"

LayerStack::LayerStack(VulkanRenderer& vulkan, GameState& state)
    : vulkan_(vulkan), state_(state) {}

LayerStack::~LayerStack() = default;

void LayerStack::push(std::unique_ptr<ILayer> layer) {
    if (!layer) return;
    layers_.push_back(std::move(layer));
    layers_.back()->onEnter();
    if (layers_.size() >= 2) {
        layers_[layers_.size() - 2]->onPause();
    }
}

void LayerStack::requestPush(std::unique_ptr<ILayer> layer) {
    if (!layer) return;
    pending_.push_back({OpType::Push, std::move(layer)});
}

void LayerStack::requestPop() {
    pending_.push_back({OpType::Pop, nullptr});
}

void LayerStack::requestReplace(std::unique_ptr<ILayer> newLayer) {
    if (!newLayer) return;
    pending_.push_back({OpType::Replace, std::move(newLayer)});
}

void LayerStack::requestQuit() {
    quitRequested_ = true;
}

void LayerStack::flushPending() {
    if (pending_.empty()) return;
    std::vector<PendingOp> ops;
    ops.swap(pending_);
    for (auto& op : ops) {
        switch (op.type) {
            case OpType::Push: {
                if (!op.layer) break;
                if (!layers_.empty()) {
                    layers_.back()->onPause();
                }
                layers_.push_back(std::move(op.layer));
                layers_.back()->onEnter();
                break;
            }
            case OpType::Pop: {
                if (layers_.empty()) break;
                layers_.back()->onExit();
                layers_.pop_back();
                if (!layers_.empty()) {
                    layers_.back()->onResume();
                }
                break;
            }
            case OpType::Replace: {
                if (!op.layer) break;
                while (!layers_.empty()) {
                    layers_.back()->onExit();
                    layers_.pop_back();
                }
                layers_.push_back(std::move(op.layer));
                layers_.back()->onEnter();
                break;
            }
        }
    }
}

void LayerStack::handleEvents(const EventBus& events) {
    if (layers_.empty()) return;
    layers_.back()->handleEvents(events, *this);
}

size_t LayerStack::findStartIndex(bool (ILayer::*blocks)() const) const {
    if (layers_.empty()) return 0;
    size_t start = layers_.size() - 1;
    while (start > 0) {
        if ((layers_[start].get()->*blocks)()) break;
        --start;
    }
    return start;
}

void LayerStack::update(float dt, const ActionState& input) {
    if (layers_.empty()) return;
    const size_t start = findStartIndex(&ILayer::blocksUpdate);
    const size_t topIndex = layers_.size() - 1;
    for (size_t i = start; i < layers_.size(); ++i) {
        layers_[i]->update(dt, i == topIndex, input);
    }
}

void LayerStack::render() {
    if (layers_.empty()) return;
    const size_t start = findStartIndex(&ILayer::blocksRender);
    const size_t end   = layers_.size();

    // ─── Phase 1C: 1 フレームの描画 (2 ステップに分解) ─────────────
    //
    // 1) Scene 構築:
    //    VulkanRenderer 内の SceneData を clear して、 下から上へ各 Layer の
    //    buildScene を呼ぶ。 各 Layer は SceneData の DrawList に直接 push。
    SceneData& scene = vulkan_.scene();
    scene.clear();
    scene.setCullingDistance(state_.settings.drawDistance);
    for (size_t i = start; i < end; ++i) {
        layers_[i]->buildScene(scene);
    }

    // 2) Frame 描画:
    //    VulkanRenderer::drawFrame に ImGui コールバックを渡す。
    //    drawFrame の中で SceneData (push 済) と FrameUniforms (camera_system が
    //    setLighting で書き込み済) を使って実際の描画コマンドを発行。
    vulkan_.drawFrame([this, start, end]() {
        for (size_t i = start; i < end; ++i) {
            layers_[i]->drawImGui();
        }
    });
}

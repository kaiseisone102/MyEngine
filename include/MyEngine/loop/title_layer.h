#pragma once
// =============================================================================
// title_layer.h — タイトル画面 Layer
// + Phase 1C: コンストラクタに VulkanRenderer& 追加、 buildScene(SceneData&)
// =============================================================================
#include <cstdint>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "loop/menu_layer_base.h"
#include "renderer/animator.h"
#include "renderer/skin_buffer_pool.h"

class AssetRegistry;
class Model;
class ILayerFactory;
class SceneData;
struct SDL_Window;

class TitleLayer : public MenuLayerBase {
   public:
    explicit TitleLayer(const LayerContext& ctx);
    ~TitleLayer() override;

    void onEnter() override;
    void onExit() override;
    void update(float dt, bool isTop, const ActionState& input) override;

    // 3D シーン (knight モデル + 床)
    void buildScene(SceneData& scene) override;

    const char* name() const override { return "Title"; }

   protected:
    std::vector<MenuItem> menuItems() const override { return {}; }
    void handleConfirm(int selectedIndex, LayerCommands& cmds) override;
    void handleBack(LayerCommands& cmds) override;
    const char* headerText() const override { return "MyEngine"; }
    float headerFontScale() const override { return 3.5f; }
    const char* hintText() const override { return "Press Enter to Start    Esc: Quit"; }
    void drawExtraUI(float winW, float winH) override;

   private:
    AssetRegistry& assets_;
    SkinBufferPool& skinPool_;
    SDL_Window* window_;
    ILayerFactory& factory_;
    const Model* knightModel_ = nullptr;
    Animator animator_;
    std::vector<glm::mat4> skinMatrices_;
    SkinBufferPool::Slot skinSlot_ = SkinBufferPool::Slot::invalid();
    uint32_t skinFrameIndex_ = 0;
    float elapsedTime_ = 0.f;
};

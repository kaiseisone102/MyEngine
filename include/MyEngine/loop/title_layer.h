#pragma once
// =============================================================================
// title_layer.h — タイトル画面 Layer
// =============================================================================
// 仕様:
//   - 中央に knight モデルを idle ポーズで表示
//   - ImGui で「MyEngine」 のタイトル + 「Press Enter to Start」 案内
//   - Enter / Return -> cmds.requestReplace(factory.createGameplayLayer())
//   - Escape         -> cmds.requestQuit()
//
// 依存:
//   render::SceneRenderer  : 描画 (ISceneSource として渡す)
//   AssetRegistry          : knight モデルとアニメ取得
//   SkinBufferPool         : ボーン行列バッファ確保
//   SDL_Window*            : ウィンドウサイズ取得 (aspect 計算)
//   ILayerFactory          : 次の Layer 生成
//
// GameState には依存しない (TitleLayer 専用リソースのみ持つ)。
// =============================================================================

#include <cstdint>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "loop/layer.h"
#include "renderer/animator.h"
#include "renderer/skin_buffer_pool.h"
#include "scene/scene_source.h"

class AssetRegistry;
class Model;
class ILayerFactory;
struct SDL_Window;

namespace render {
class SceneRenderer;
}

class TitleLayer : public ILayer, public scene::ISceneSource {
   public:
    TitleLayer(render::SceneRenderer& renderer, AssetRegistry& assets, SkinBufferPool& skinPool,
               SDL_Window* window, ILayerFactory& factory);
    ~TitleLayer() override;

    // ─── ILayer ─────────────────────────────────────────────────
    void onEnter() override;
    void onExit() override;

    void handleEvents(const EventBus& events, LayerCommands& cmds) override;
    void update(float dt, bool isTop) override;
    void render() override;

    // タイトル画面はバックグラウンドの Layer を停止・隠蔽 (現状下に何も無いが)。
    bool blocksUpdate() const override { return true; }
    bool blocksRender() const override { return true; }

    // ─── ISceneSource ───────────────────────────────────────────
    void buildScene(scene::Scene& scene) override;

   private:
    // 依存
    render::SceneRenderer& renderer_;
    AssetRegistry& assets_;
    SkinBufferPool& skinPool_;
    SDL_Window* window_;
    ILayerFactory& factory_;

    // タイトル独自リソース
    const Model* knightModel_ = nullptr;
    Animator animator_;
    std::vector<glm::mat4> skinMatrices_;
    SkinBufferPool::Slot skinSlot_ = SkinBufferPool::Slot::invalid();

    // フレーム制御
    uint32_t skinFrameIndex_ = 0;
    float    elapsedTime_    = 0.f;  // タイトル開始からの経過秒数 (アニメ用)
};

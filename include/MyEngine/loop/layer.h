#pragma once
// =============================================================================
// layer.h — ILayer + LayerCommands + MouseCapturePolicy
// + Phase 1C: buildScene(scene::Scene&) → buildScene(SceneData&)
// =============================================================================
// 描画責務:
//   - buildScene(scene) — 3D シーン (SceneData) への寄与
//   - drawImGui()       — ImGui への寄与
//   LayerStack が両方を統合し、 SceneRenderer に 1 フレームの描画を依頼。
// =============================================================================
#include <memory>

class SceneData;
class EventBus;
struct ActionState;

// マウスキャプチャポリシー:
//   Locked    — マウスをウィンドウに capture して相対移動を取得 (FPS/TPS ゲーム)
//   Released  — マウスを解放 (メニュー画面、 ダイアログ等)
enum class MouseCapturePolicy {
    Locked,
    Released,
};

class ILayer;

class LayerCommands {
   public:
    virtual ~LayerCommands() = default;
    virtual void requestPush(std::unique_ptr<ILayer> layer) = 0;
    virtual void requestPop() = 0;
    virtual void requestReplace(std::unique_ptr<ILayer> newLayer) = 0;
    virtual void requestQuit() = 0;
};

class ILayer {
   public:
    virtual ~ILayer() = default;

    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onPause() {}
    virtual void onResume() {}

    virtual void handleEvents(const EventBus& events, LayerCommands& cmds) = 0;
    virtual void update(float dt, bool isTop, const ActionState& input) = 0;

    // ─── 描画責務 (1 フレームに 1 回、 LayerStack が下から上へ順次呼ぶ) ─
    virtual void buildScene(SceneData& scene) { (void)scene; }
    virtual void drawImGui() {}

    // 下の Layer の update を止めるか
    virtual bool blocksUpdate() const { return true; }

    // 下の Layer の描画 (buildScene/drawImGui) を止めるか
    //   true  : この Layer が完全に画面を埋める
    //   false : 下の Layer も描かれる (= オーバーレイ)
    virtual bool blocksRender() const { return true; }

    virtual MouseCapturePolicy mouseCapturePolicy() const {
        return MouseCapturePolicy::Released;
    }

    virtual const char* name() const { return "Layer"; }
};

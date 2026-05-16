#pragma once
// =============================================================================
// scene_source.h — シーン構築のインターフェース
// =============================================================================
// Layer (TitleLayer / GameplayLayer 等) はこれを実装し、
// 毎フレーム自分のシーン内容を Scene に積む。
//
// SceneRenderer.render(source) を呼ぶと:
//   1. Scene が clear される
//   2. source.buildScene(scene) が呼ばれて Layer がノードを積む
//   3. SceneRenderer が Scene を巡回して RenderQueue -> SceneData に流す
//   4. Vulkan で描画
//
// テスト容易性:
//   Layer の単体テストでは MockScene を渡して、 「buildScene が期待した
//   ノード数を積んだか」 等を検証できる (Scene は値オブジェクトなので
//   Mock 不要、 そのまま使える)。
// =============================================================================

namespace scene {

class Scene;

class ISceneSource {
   public:
    virtual ~ISceneSource() = default;

    // 自分のシーン内容を scene に積む。 毎フレーム呼ばれる。
    // scene は呼び出し側で clear 済み (空の状態) で渡される。
    virtual void buildScene(Scene& scene) = 0;
};

}  // namespace scene

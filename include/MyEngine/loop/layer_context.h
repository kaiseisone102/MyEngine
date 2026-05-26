#pragma once
// =============================================================================
// layer_context.h — レイヤーに渡す依存をまとめた束 (LayerCommands と対)
//   LayerCommands = 層が「要求」できること / LayerContext = 層が「触れる」もの。
//   参照だけを保持する軽量な束。static グローバルにはせず、factory が値で作って
//   各層コンストラクタに const& で渡し、層はコンストラクタ内で必要分を
//   自分のメンバへ取り込む (一時オブジェクト消滅後もダングリングしない)。
// =============================================================================

struct GameState;
class SceneRenderer;
class VulkanRenderer;
class ILayerFactory;

struct LayerContext {
    GameState&      state;
    SceneRenderer&  sceneRenderer;
    VulkanRenderer& vulkan;
    ILayerFactory&  factory;
};
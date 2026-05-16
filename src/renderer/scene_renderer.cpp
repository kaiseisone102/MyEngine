// =============================================================================
// scene_renderer.cpp — 実装 (オーバーレイ対応 + デバッグ線クリア)
// =============================================================================
#include "renderer/scene_renderer.h"

#include "renderer/vulkan_renderer.h"

namespace render {

void SceneRenderer::renderFrame(SceneBuilder sceneBuilder, UIDrawer uiDrawer) {
    // ─── 1. Scene 構築 (複数 Layer の寄与を合成) ────────────────
    scene_.clear();
    // デバッグ線も同じく毎フレームクリアする
    // (各 Layer の buildScene で再度 addLine 等が呼ばれて積まれる)
    vulkan_.debugLines().clear();
    // HUD も毎フレームクリアする
    // (各 Layer の buildScene で HudSystem 等が再度矩形を積む)
    vulkan_.hud().clear();

    if (sceneBuilder) {
        sceneBuilder(scene_);
    }

    // ─── 2. Scene を巡回して RenderQueue に enqueue ─────────────
    queue_.clear();
    scene_.traverse([&](const scene::SceneNode& node, const glm::mat4& worldMatrix) {
        if (const scene::IRenderable* r = node.renderable()) {
            r->enqueue(queue_, worldMatrix);
        }
    });

    // ─── 3. RenderQueue → SceneData ────────────────────────────
    SceneData& sd = vulkan_.scene();
    sd.clearObjects();

    sd.setViewProjection(scene_.camera().view, scene_.camera().projection);
    sd.setLightingParams(scene_.light().target + scene_.light().offset, scene_.light().color,
                         scene_.camera().worldPosition, scene_.env().ambient,
                         scene_.env().specular);
    sd.setShadowParams(scene_.env().shadowStrength, scene_.env().shadowBias);
    sd.setPlayerCenter(scene_.light().target);

    for (const RenderCommand& cmd : queue_.commands()) {
        switch (cmd.kind) {
            case RenderCommand::Kind::Cube:
                sd.addMeshObject(cmd.worldMatrix, cmd.meshMaterial);
                break;
            case RenderCommand::Kind::StaticMesh:
                sd.addStaticModelObject(cmd.worldMatrix, cmd.model);
                break;
            case RenderCommand::Kind::SkinnedMesh:
                sd.addModelObject(cmd.worldMatrix, cmd.skinOffset, cmd.model);
                break;
            case RenderCommand::Kind::Terrain:
                sd.addTerrainObject(cmd.worldMatrix, cmd.terrain, cmd.meshMaterial);
                break;
        }
    }

    sd.sortModelDrawListBySourceModel();
    sd.sortStaticModelDrawListBySourceModel();

    // ─── 4. 実描画 (uiDrawer の中で全 Layer の drawImGui が呼ばれる) ──
    vulkan_.drawFrame(uiDrawer);
}

}  // namespace render

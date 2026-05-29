// =============================================================================
// title_layer.cpp - title screen Layer
// + Phase 1C: buildScene populates SceneData directly
//   - LightingUBO is built here and passed to vulkan().setLighting()
//   - knight model -> modelDrawListOpaque (SkinnedDrawItem)
//   - ground       -> terrainDrawListOpaque (shared flat grass terrain)
// =============================================================================
#define NOMINMAX
#include "loop/title_layer.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <cmath>
#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include "core/action_state.h"
#include "loop/layer_factory.h"
#include "renderer/animation.h"
#include "renderer/asset_registry.h"
#include "renderer/frame_uniforms.h"
#include "renderer/projection.h"
#include "renderer/model.h"
#include "renderer/shadow_light.h"
#include "renderer/vulkan_renderer.h"
#include "core/game_state.h"
#include "scene/scene_data.h"
#include "scene/scene_renderer.h"

TitleLayer::TitleLayer(const LayerContext& ctx)
    : MenuLayerBase(ctx),
      assets_(ctx.vulkan.assets()),
      skinPool_(ctx.vulkan.skinBufferPool()),
      window_(ctx.state.runtime.window),
      factory_(ctx.factory) {}

TitleLayer::~TitleLayer() {
    if (skinSlot_.valid()) {
        skinPool_.release(skinSlot_);
        skinSlot_ = SkinBufferPool::Slot::invalid();
    }
}

void TitleLayer::onEnter() {
    std::cout << "[TitleLayer] enter\n";
    knightModel_ = assets_.getModel("knight");
    if (!knightModel_ || !knightModel_->hasSkeleton()) {
        std::cerr << "[TitleLayer] WARNING: knight model not available\n";
        return;
    }
    const AnimationClip* clip = assets_.getAnimation("idle");
    if (!clip) {
        std::cerr << "[TitleLayer] WARNING: idle anim not available\n";
    }
    animator_.bind(&knightModel_->skeleton(), clip);
    skinMatrices_.assign(knightModel_->skeleton().boneCount(), glm::mat4(1.f));
    skinSlot_ = skinPool_.allocate();
    if (!skinSlot_.valid()) {
        std::cerr << "[TitleLayer] WARNING: SkinBufferPool allocation failed\n";
    }
}

void TitleLayer::onExit() {
    std::cout << "[TitleLayer] exit\n";
    if (skinSlot_.valid()) {
        skinPool_.release(skinSlot_);
        skinSlot_ = SkinBufferPool::Slot::invalid();
    }
}

void TitleLayer::handleConfirm(int selectedIndex, LayerCommands& cmds) {
    (void)selectedIndex;
    cmds.requestPush(factory_.createModeSelectLayer());
}

void TitleLayer::handleBack(LayerCommands& cmds) { cmds.requestQuit(); }

void TitleLayer::update(float dt, bool isTop, const ActionState& input) {
    (void)isTop;
    (void)input;
    elapsedTime_ += dt;
    if (animator_.ready()) {
        animator_.update(dt);
        animator_.computeSkinMatrices(skinMatrices_);
    }
}

void TitleLayer::buildScene(SceneData& scene) {
    // ----- 1. Camera / projection -----
    int winW = 1280, winH = 720;
    if (window_) SDL_GetWindowSize(window_, &winW, &winH);
    const float aspect = (winH > 0) ? static_cast<float>(winW) / static_cast<float>(winH) : 1.f;

    const glm::vec3 cameraPos{2.5f, 2.0f, 4.0f};
    const glm::vec3 lookAt{0.f, 1.0f, 0.f};
    const glm::mat4 view = glm::lookAt(cameraPos, lookAt, glm::vec3{0.f, 1.f, 0.f});
    // Reverse-Z + infinite far. Vulkan Y flip is baked into the helper.
    glm::mat4 proj = makeReversedZInfinitePerspective(glm::radians(45.f), aspect, 0.1f);

    // ----- 2. Light view-projection (for shadow map) -----
    const glm::vec3 lightTarget{0.f, 0.5f, 0.f};
    const glm::vec3 lightOffset{8.f, 15.f, 8.f};
    const glm::vec3 lightPos = lightTarget + lightOffset;
    const glm::mat4 lightView =
        glm::lookAt(lightPos, lightTarget, glm::vec3(0.f, 1.f, 0.f));
    const glm::mat4 lightProj = shadow_light::directionalLightProj();

    // ----- 3. Build LightingUBO and submit -----
    FrameUniforms::LightingUBO ubo{};
    ubo.view = view;
    ubo.proj = proj;
    ubo.lightVP = lightProj * lightView;
    ubo.lightDir = glm::vec4(glm::normalize(lightTarget - lightPos), 0.f);
    ubo.lightColor = glm::vec4(1.f, 1.f, 1.f, 0.f);
    ubo.ambient = glm::vec4(0.25f, 0.25f, 0.25f, 0.f);
    ubo.viewPos = glm::vec4(cameraPos, 0.f);
    ubo.shadowParams = glm::vec4(0.6f, 0.f, 0.f, 0.f);  // x = strength
    vulkan().setLighting(ubo);

    // ----- 4. Knight model (skinned) -----
    if (knightModel_ && skinSlot_.valid() && animator_.ready()) {
        if (!skinMatrices_.empty()) {
            skinPool_.update(skinFrameIndex_, skinSlot_, skinMatrices_);
        }
        glm::mat4 m(1.f);
        m = glm::translate(m, glm::vec3(0.f, 0.f, 0.f));
        m = glm::scale(m, glm::vec3(0.6f, 1.0f, 0.6f));

        SkinnedDrawItem item;
        item.model = m;
        item.sourceModel = knightModel_;
        item.skinOffset = static_cast<int>(skinSlot_.boneOffset);
        item.alpha = 1.f;
        scene.modelDrawListOpaque().push_back(item);
    }

    // ----- 5. Ground: shared flat grass terrain (same look as in-game) -----
    {
        TerrainDrawItem item;
        item.model = glm::mat4(1.f);
        item.terrain = &assets_.sharedFlatTerrain();
        item.material = assets_.sharedFlatTerrain().material();
        item.alpha = 1.f;
        scene.terrainDrawListOpaque().push_back(item);
    }

    skinFrameIndex_ = (skinFrameIndex_ + 1) % FrameSync::MAX_FRAMES_IN_FLIGHT;
}

void TitleLayer::drawExtraUI(float winW, float winH) {
    const float blinkAlpha = 0.5f + 0.5f * std::sin(elapsedTime_ * 3.0f);
    ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.78f), ImGuiCond_Always,
                              ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##TitlePrompt", nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                      ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::SetWindowFontScale(1.8f);
    ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, blinkAlpha), "Press Enter to Start");
    ImGui::End();
}

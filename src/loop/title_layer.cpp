// =============================================================================
// title_layer.cpp — タイトル画面 Layer 実装
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

#include "loop/layer_factory.h"
#include "renderer/animation.h"
#include "renderer/asset_registry.h"
#include "renderer/model.h"
#include "renderer/scene_renderer.h"
#include "scene/renderable.h"
#include "scene/scene.h"

TitleLayer::TitleLayer(render::SceneRenderer& renderer, AssetRegistry& assets,
                       SkinBufferPool& skinPool, SDL_Window* window, ILayerFactory& factory)
    : MenuLayerBase(renderer),
      assets_(assets),
      skinPool_(skinPool),
      window_(window),
      factory_(factory) {}

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
        std::cerr << "[TitleLayer] WARNING: idle anim not available, using bind pose\n";
    }

    animator_.bind(&knightModel_->skeleton(), clip);
    skinMatrices_.assign(knightModel_->skeleton().boneCount(), glm::mat4(1.f));

    skinSlot_ = skinPool_.allocate();
    if (!skinSlot_.valid()) {
        std::cerr << "[TitleLayer] WARNING: SkinBufferPool allocation failed\n";
    } else {
        std::cout << "[TitleLayer] knight skinSlot offset=" << skinSlot_.boneOffset << "\n";
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
    (void)selectedIndex;  // 項目なしなので -1
    cmds.requestPush(factory_.createChoiceOverlay(
        this, "Start the game?", {"Yes", "No"},
        [&](int idx, LayerCommands& cmds) {
            cmds.requestPop();
            if (idx == 0) cmds.requestPush(factory_.createModeSelectLayer());
        }
    ));
}

void TitleLayer::handleBack(LayerCommands& cmds) {
    std::cout << "[TitleLayer] back → quit\n";
    cmds.requestQuit();
}

void TitleLayer::update(float dt, bool isTop) {
    (void)isTop;
    elapsedTime_ += dt;

    if (animator_.ready()) {
        animator_.update(dt);
        animator_.computeSkinMatrices(skinMatrices_);
    }
}

void TitleLayer::buildScene(scene::Scene& scene) {
    int winW = 1280, winH = 720;
    if (window_) SDL_GetWindowSize(window_, &winW, &winH);
    const float aspect = (winH > 0) ? static_cast<float>(winW) / static_cast<float>(winH) : 1.f;

    const glm::vec3 cameraPos{2.5f, 2.0f, 4.0f};
    const glm::vec3 lookAt{0.f, 1.0f, 0.f};
    const glm::mat4 view = glm::lookAt(cameraPos, lookAt, glm::vec3{0.f, 1.f, 0.f});

    glm::mat4 proj = glm::perspective(glm::radians(45.f), aspect, 0.1f, 200.f);
    proj[1][1] *= -1.f;

    scene.setCameraView(view);
    scene.setCameraProjection(proj);
    scene.setCameraWorldPosition(cameraPos);

    scene.setLightTarget({0.f, 0.5f, 0.f});
    scene.setLightOffset({8.f, 15.f, 8.f});
    scene.setLightColor({1.f, 1.f, 1.f});

    scene::EnvSettings env;
    env.ambient        = 0.25f;
    env.specular       = 0.5f;
    env.shadowStrength = 0.6f;
    env.shadowBias     = 0.0015f;
    scene.setEnv(env);

    if (knightModel_ && skinSlot_.valid() && animator_.ready()) {
        if (!skinMatrices_.empty()) {
            skinPool_.update(skinFrameIndex_, skinSlot_, skinMatrices_);
        }

        scene::Transform t;
        t.position = {0.f, 0.f, 0.f};
        t.yaw      = 0.f;
        t.scale    = {0.6f, 1.0f, 0.6f};

        auto* node = scene.addRoot(t);
        node->setRenderable<scene::SkinnedMeshRenderable>(
            knightModel_, static_cast<int>(skinSlot_.boneOffset));
    }

    {
        scene::Transform t;
        t.position = {0.f, -0.2f, 0.f};
        t.scale    = {20.f, 0.2f, 20.f};
        auto* node = scene.addRoot(t);
        node->setRenderable<scene::CubeRenderable>();
    }

    skinFrameIndex_ = (skinFrameIndex_ + 1) % FrameSync::MAX_FRAMES_IN_FLIGHT;
}

void TitleLayer::drawExtraUI(float winW, float winH) {
    // タイトル特有: 「Press Enter to Start」 を点滅させて画面中央下に描画
    // (ヘッダ「MyEngine」 + 操作ヒントは MenuLayerBase が描く)
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

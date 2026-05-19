// =============================================================================
// default_layer_factory.cpp
// + Phase 1C: 各 Layer のコンストラクタ第 2 引数として VulkanRenderer& を渡す
//             (MenuLayerBase が SceneRenderer& + VulkanRenderer& の 2 引数を要求)
// =============================================================================
#include "loop/default_layer_factory.h"

#include <utility>

#include "core/game_state.h"
#include "loop/choice_overlay_layer.h"
#include "loop/game_over_layer.h"
#include "loop/gameplay_layer.h"
#include "loop/graphics_settings_layer.h"
#include "loop/key_config_layer.h"
#include "loop/mode_select_layer.h"
#include "loop/settings_layer.h"
#include "loop/title_layer.h"
#include "renderer/skin_buffer_pool.h"
#include "renderer/vulkan_renderer.h"
#include "scene/scene_renderer.h"

std::unique_ptr<ILayer> DefaultLayerFactory::createTitleLayer() {
    return std::make_unique<TitleLayer>(
        renderer_,
        state_.worldState.data.vulkan,
        state_.worldState.data.vulkan.assets(),
        state_.worldState.data.vulkan.skinBufferPool(),
        state_.runtime.window,
        *this);
}

std::unique_ptr<ILayer> DefaultLayerFactory::createModeSelectLayer() {
    return std::make_unique<ModeSelectLayer>(renderer_, state_.worldState.data.vulkan, *this);
}

std::unique_ptr<ILayer> DefaultLayerFactory::createGameplayLayer(StageId initialStage) {
    return std::make_unique<GameplayLayer>(state_, renderer_,
                                             state_.worldState.data.vulkan, *this,
                                             gravity_, jumpSpeed_, initialStage);
}

std::unique_ptr<ILayer> DefaultLayerFactory::createGameOverLayer() {
    return std::make_unique<GameOverLayer>(renderer_, state_.worldState.data.vulkan,
                                             *this, state_);
}

std::unique_ptr<ILayer> DefaultLayerFactory::createSettingsLayer() {
    return std::make_unique<SettingsLayer>(renderer_, state_.worldState.data.vulkan,
                                             state_, *this);
}

std::unique_ptr<ILayer> DefaultLayerFactory::createKeyConfigLayer() {
    return std::make_unique<KeyConfigLayer>(renderer_, state_.worldState.data.vulkan,
                                              state_, *this);
}

std::unique_ptr<ILayer> DefaultLayerFactory::createGraphicsSettingsLayer() {
    return std::make_unique<GraphicsSettingsLayer>(renderer_, state_.worldState.data.vulkan,
                                                     state_, *this);
}

std::unique_ptr<ILayer> DefaultLayerFactory::createChoiceOverlay(
    std::string prompt, std::vector<std::string> choices,
    std::function<void(int idx, LayerCommands& cmds)> onChoice,
    MenuLayerBase::MenuLayout layout) {
    return std::make_unique<ChoiceOverlayLayer>(renderer_, state_.worldState.data.vulkan,
                                                  std::move(prompt), std::move(choices),
                                                  std::move(onChoice), layout);
}

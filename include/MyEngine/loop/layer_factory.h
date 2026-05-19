#pragma once
// =============================================================================
// layer_factory.h
// =============================================================================
// 拡張:
//   - createGraphicsSettingsLayer — グラフィック設定 Layer
// =============================================================================

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "loop/menu_layer_base.h"
#include "world/stage_id.h"

class ILayer;
class LayerCommands;

class ILayerFactory {
   public:
    virtual ~ILayerFactory() = default;

    virtual std::unique_ptr<ILayer> createTitleLayer() = 0;
    virtual std::unique_ptr<ILayer> createModeSelectLayer() = 0;
    virtual std::unique_ptr<ILayer> createGameplayLayer(StageId initialStage) = 0;
    virtual std::unique_ptr<ILayer> createGameOverLayer() = 0;
    virtual std::unique_ptr<ILayer> createSettingsLayer() = 0;
    virtual std::unique_ptr<ILayer> createKeyConfigLayer() = 0;
    virtual std::unique_ptr<ILayer> createGraphicsSettingsLayer() = 0;

    virtual std::unique_ptr<ILayer> createChoiceOverlay(
        std::string prompt, std::vector<std::string> choices,
        std::function<void(int idx, LayerCommands& cmds)> onChoice,
        MenuLayerBase::MenuLayout layout = MenuLayerBase::MenuLayout::Horizontal) = 0;
};

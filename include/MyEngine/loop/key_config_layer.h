#pragma once
// =============================================================================
// key_config_layer.h 窶・繧ｭ繝ｼ繧ｳ繝ｳ繝輔ぅ繧ｰ逕ｻ髱｢ Layer (譏守､ｺ菫晏ｭ伜ｯｾ蠢・
// =============================================================================
// 莉墓ｧ・
//   - 繧ｭ繝ｼ繝舌う繝ｳ繝牙､画峩縺ｯ Save 縺吶ｋ縺ｾ縺ｧ kbDevice 縺ｫ蜿肴丐縺励↑縺・//   - Save 鬆・岼縺ｧ豌ｸ邯壼喧 + kbDevice 驕ｩ逕ｨ
//   - Back 譎ゅ↓譛ｪ菫晏ｭ伜､画峩縺ゅｌ縺ｰ ChoiceOverlay 縺ｧ遐ｴ譽・｢ｺ隱・// =============================================================================

#include <string>
#include <vector>

#include "core/game_settings.h"
#include "core/input_binding.h"
#include "core/key_mapping.h"
#include "loop/menu_layer_base.h"

class GameState;
class ILayerFactory;

class KeyConfigLayer : public MenuLayerBase {
   public:
    enum class Action {
        MoveForward,
        MoveBack,
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        Sprint,
        Jump,
        ToggleCamera,
        Attack,
        StrongAttack,
        Guard,
        Count
    };

    KeyConfigLayer(SceneRenderer& renderer, VulkanRenderer& vulkan, GameState& state, ILayerFactory& factory);
    ~KeyConfigLayer() override;

    void onEnter() override;
    void onExit() override;

    void handleEvents(const EventBus& events, LayerCommands& cmds) override;
    void update(float dt, bool isTop, const ActionState& input) override;

    const char* name() const override { return "KeyConfig"; }

   protected:
    std::vector<MenuItem> menuItems() const override;

    void handleConfirm(int selectedIndex, LayerCommands& cmds) override;
    void handleBack(LayerCommands& cmds) override;

    const char* headerText() const override { return "Key Bindings"; }
    float headerFontScale() const override { return 2.5f; }
    const char* hintText() const override {
        return waiting_ ? "Press any key / mouse button to bind ...    Esc: Cancel"
                        : "Up/Down: Navigate    Enter: Bind/Save/Back    Esc: Back";
    }

    void drawBackground(float winW, float winH) override;
    void drawExtraUI(float winW, float winH) override;

   private:
    static const char* actionLabel(Action a);

    InputBinding* bindingPtr(KeyMapping& mapping, Action a);
    const InputBinding* bindingPtr(const KeyMapping& mapping, Action a) const;

    void applyNewBinding(Action target, InputBinding newBinding);
    void resetToDefault();

    void doSave();
    void discardChanges();

    GameState& state_;
    ILayerFactory& factory_;

    // 邱ｨ髮・燕縺ｮ繧ｹ繝翫ャ繝励す繝ｧ繝・ヨ
    GameSettings snapshot_;
    bool hasUnsavedChanges_ = false;

    bool waiting_ = false;
    Action waitingAction_ = Action::MoveForward;

    std::string warningText_;
    float warningTimer_ = 0.f;
    static constexpr float kWarningDuration = 3.0f;
};

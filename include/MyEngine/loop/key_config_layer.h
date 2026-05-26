#pragma once
// =============================================================================
// key_config_layer.h  Eキーコンフィグ画面 Layer (明示保存対忁E
// =============================================================================
// 仕槁E
//   - キーバインド変更は Save するまで kbDevice に反映しなぁE//   - Save 頁E��で永続化 + kbDevice 適用
//   - Back 時に未保存変更あれば ChoiceOverlay で破棁E��誁E// =============================================================================

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

    explicit KeyConfigLayer(const LayerContext& ctx);
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

    // 編雁E��のスナップショチE��
    GameSettings snapshot_;
    bool hasUnsavedChanges_ = false;

    bool waiting_ = false;
    Action waitingAction_ = Action::MoveForward;

    std::string warningText_;
    float warningTimer_ = 0.f;
    static constexpr float kWarningDuration = 3.0f;
};

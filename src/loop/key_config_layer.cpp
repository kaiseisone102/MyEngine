// =============================================================================
// key_config_layer.cpp
// =============================================================================
#define NOMINMAX
#include "loop/key_config_layer.h"

#include <imgui.h>

#include <algorithm>
#include <iostream>

#include "core/action_state.h"
#include "core/event_bus.h"
#include "core/game_state.h"
#include "loop/layer_factory.h"
#include "scene/scene_renderer.h"
#include "renderer/vulkan_renderer.h"

namespace {

constexpr int kActionCount = static_cast<int>(KeyConfigLayer::Action::Count);
constexpr int kResetIndex = kActionCount;
constexpr int kSaveIndex = kActionCount + 1;
constexpr int kBackIndex = kActionCount + 2;

}  // namespace

KeyConfigLayer::KeyConfigLayer(const LayerContext& ctx)
    : MenuLayerBase(ctx), state_(ctx.state), factory_(ctx.factory) {}

KeyConfigLayer::~KeyConfigLayer() = default;

void KeyConfigLayer::onEnter() {
    std::cout << "[KeyConfigLayer] enter\n";
    snapshot_ = state_.settings;
    hasUnsavedChanges_ = false;
    setSelectedIndex(0);
    waiting_ = false;
    warningText_.clear();
    warningTimer_ = 0.f;
}

void KeyConfigLayer::onExit() { std::cout << "[KeyConfigLayer] exit\n"; }

const char* KeyConfigLayer::actionLabel(Action a) {
    switch (a) {
        case Action::MoveForward:
            return "Move Forward";
        case Action::MoveBack:
            return "Move Back";
        case Action::MoveLeft:
            return "Move Left";
        case Action::MoveRight:
            return "Move Right";
        case Action::MoveUp:
            return "Move Up (FPS)";
        case Action::MoveDown:
            return "Move Down (FPS)";
        case Action::Sprint:
            return "Sprint";
        case Action::Jump:
            return "Jump";
        case Action::ToggleCamera:
            return "Toggle Camera";
        case Action::Attack:
            return "Attack";
        case Action::StrongAttack:
            return "Strong Attack";
        case Action::Guard:
            return "Guard";
        case Action::Count:
            return "?";
    }
    return "?";
}

InputBinding* KeyConfigLayer::bindingPtr(KeyMapping& m, Action a) {
    switch (a) {
        case Action::MoveForward:
            return &m.moveForward;
        case Action::MoveBack:
            return &m.moveBack;
        case Action::MoveLeft:
            return &m.moveLeft;
        case Action::MoveRight:
            return &m.moveRight;
        case Action::MoveUp:
            return &m.moveUp;
        case Action::MoveDown:
            return &m.moveDown;
        case Action::Sprint:
            return &m.sprint;
        case Action::Jump:
            return &m.jump;
        case Action::ToggleCamera:
            return &m.toggleCamera;
        case Action::Attack:
            return &m.attack;
        case Action::StrongAttack:
            return &m.strongAttack;
        case Action::Guard:
            return &m.guard;
        case Action::Count:
            return nullptr;
    }
    return nullptr;
}

const InputBinding* KeyConfigLayer::bindingPtr(const KeyMapping& m, Action a) const {
    return const_cast<KeyConfigLayer*>(this)->bindingPtr(const_cast<KeyMapping&>(m), a);
}

std::vector<MenuItem> KeyConfigLayer::menuItems() const {
    std::vector<MenuItem> items;
    items.reserve(kActionCount + 3);

    const KeyMapping& mapping = state_.settings.keyMapping;
    for (int i = 0; i < kActionCount; ++i) {
        const Action a = static_cast<Action>(i);
        const InputBinding* b = bindingPtr(mapping, a);
        const std::string label = actionLabel(a);

        std::string right;
        if (waiting_ && waitingAction_ == a) {
            right = "[Press any key...]";
        } else if (b) {
            right = "[" + b->displayName() + "]";
        } else {
            right = "[---]";
        }
        items.emplace_back(label, std::move(right));
    }

    items.emplace_back("Reset to Default");
    items.emplace_back(hasUnsavedChanges_ ? "Save *" : "Save");
    items.emplace_back("Back");
    return items;
}

void KeyConfigLayer::handleConfirm(int selectedIndex, LayerCommands& cmds) {
    if (selectedIndex == kBackIndex) {
        handleBack(cmds);
        return;
    }
    if (selectedIndex == kSaveIndex) {
        doSave();
        return;
    }
    if (selectedIndex == kResetIndex) {
        resetToDefault();
        return;
    }
    if (selectedIndex >= 0 && selectedIndex < kActionCount) {
        waiting_ = true;
        waitingAction_ = static_cast<Action>(selectedIndex);
        std::cout << "[KeyConfigLayer] waiting for binding: " << actionLabel(waitingAction_)
                  << "\n";
    }
}

void KeyConfigLayer::handleEvents(const EventBus& events, LayerCommands& cmds) {
    if (waiting_) {
        if (findEvent<MenuBackRequested>(events)) {
            std::cout << "[KeyConfigLayer] binding canceled\n";
            waiting_ = false;
            return;
        }

        for (const GameEvent& ev : events.events()) {
            if (const auto* rk = std::get_if<RawKeyPressed>(&ev)) {
                if (rk->scancode == SDL_SCANCODE_ESCAPE) continue;
                applyNewBinding(waitingAction_,
                                InputBinding::key(static_cast<SDL_Scancode>(rk->scancode)));
                waiting_ = false;
                return;
            }
            if (const auto* rm = std::get_if<RawMouseButtonPressed>(&ev)) {
                applyNewBinding(waitingAction_,
                                InputBinding::mouse(static_cast<Uint8>(rm->button)));
                waiting_ = false;
                return;
            }
        }

        if (findEvent<QuitRequested>(events)) {
            cmds.requestQuit();
            return;
        }
        if (findEvent<WindowResizeRequested>(events)) {
            vulkan().onResize();
            return;
        }
        return;
    }

    MenuLayerBase::handleEvents(events, cmds);
}

void KeyConfigLayer::update(float dt, bool isTop, const ActionState& input) {
    (void)isTop;
    (void)input;
    if (warningTimer_ > 0.f) {
        warningTimer_ -= dt;
        if (warningTimer_ <= 0.f) {
            warningTimer_ = 0.f;
            warningText_.clear();
        }
    }
}

void KeyConfigLayer::applyNewBinding(Action target, InputBinding newBinding) {
    auto& mapping = state_.settings.keyMapping;

    std::string conflictLabel;
    for (int i = 0; i < kActionCount; ++i) {
        const Action a = static_cast<Action>(i);
        if (a == target) continue;
        InputBinding* b = bindingPtr(mapping, a);
        if (!b) continue;
        if (*b == newBinding) {
            conflictLabel = actionLabel(a);
            *b = InputBinding::none();
        }
    }

    InputBinding* tb = bindingPtr(mapping, target);
    if (tb) *tb = newBinding;

    hasUnsavedChanges_ = true;

    if (!conflictLabel.empty()) {
        warningText_ = newBinding.displayName() + " was unbound from '" + conflictLabel + "'";
        warningTimer_ = kWarningDuration;
        std::cout << "[KeyConfigLayer] " << warningText_ << "\n";
    }

    std::cout << "[KeyConfigLayer] bound " << actionLabel(target) << " -> "
              << newBinding.displayName() << " (unsaved)\n";
}

void KeyConfigLayer::resetToDefault() {
    state_.settings.keyMapping = KeyMapping{};
    hasUnsavedChanges_ = true;
    warningText_ = "Reset to defaults (unsaved)";
    warningTimer_ = kWarningDuration;
    std::cout << "[KeyConfigLayer] reset to default (unsaved)\n";
}

void KeyConfigLayer::doSave() {
    state_.settings.keyMappingDirty = true;
    state_.settings.persistDirty = true;

    snapshot_ = state_.settings;
    hasUnsavedChanges_ = false;

    warningText_ = "Saved";
    warningTimer_ = kWarningDuration;
    std::cout << "[KeyConfigLayer] saved\n";
}

void KeyConfigLayer::discardChanges() {
    state_.settings = snapshot_;
    hasUnsavedChanges_ = false;
    std::cout << "[KeyConfigLayer] reverted to snapshot\n";
}

void KeyConfigLayer::handleBack(LayerCommands& cmds) {
    if (!hasUnsavedChanges_) {
        std::cout << "[KeyConfigLayer] Back (no unsaved changes)\n";
        cmds.requestPop();
        return;
    }

    std::cout << "[KeyConfigLayer] Back with unsaved changes -> confirm dialog\n";
    cmds.requestPush(factory_.createChoiceOverlay(
        "Discard unsaved changes?", {"Yes", "No"}, [this](int idx, LayerCommands& c) {
            c.requestPop();
            if (idx == 0) {
                std::cout << "[KeyConfigLayer] discarding unsaved changes\n";
                discardChanges();
                c.requestPop();
            } else {
                std::cout << "[KeyConfigLayer] continue editing\n";
            }
        }));
}

void KeyConfigLayer::drawBackground(float winW, float winH) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(winW, winH),
                                                  IM_COL32(0, 0, 0, 255));
}

void KeyConfigLayer::drawExtraUI(float winW, float winH) {
    if (warningTimer_ <= 0.f || warningText_.empty()) return;

    const float alpha = std::min(1.f, warningTimer_ / 0.5f);

    ImGui::SetNextWindowPos(ImVec2(winW * 0.5f, winH * 0.82f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##KeyConfigWarning", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, alpha), "%s", warningText_.c_str());
    ImGui::End();
}

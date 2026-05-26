#pragma once
// =============================================================================
// menu_layer_base.h — メニュー画面の共通基底
// + Phase 1C: SceneRenderer& に加えて VulkanRenderer& も保持
//             (renderer().vulkan() の代替経路として vulkan() を直接公開)
// =============================================================================
#include <string>
#include <utility>
#include <vector>
#include "loop/layer.h"
#include "loop/layer_context.h"

class SceneRenderer;
class VulkanRenderer;

struct MenuItem {
    std::string label;
    std::string rightText;
    MenuItem() = default;
    MenuItem(const char* l) : label(l) {}
    MenuItem(std::string l) : label(std::move(l)) {}
    MenuItem(std::string l, std::string r)
        : label(std::move(l)), rightText(std::move(r)) {}
};

class MenuLayerBase : public ILayer {
   public:
    enum class MenuLayout { Vertical, Horizontal };

    explicit MenuLayerBase(const LayerContext& ctx);
    ~MenuLayerBase() override;

    void handleEvents(const EventBus& events, LayerCommands& cmds) override;
    void update(float dt, bool isTop, const ActionState& input) override {
        (void)dt;
        (void)isTop;
        (void)input;
    }
    void drawImGui() override;
    bool blocksUpdate() const override { return true; }
    bool blocksRender() const override { return true; }
    MouseCapturePolicy mouseCapturePolicy() const override {
        return MouseCapturePolicy::Released;
    }

   protected:
    virtual std::vector<MenuItem> menuItems() const { return {}; }
    virtual void handleConfirm(int selectedIndex, LayerCommands& cmds) = 0;
    virtual void handleBack(LayerCommands& cmds) {
        cmds.requestPop();
    }
    virtual void handleAdjust(int selectedIndex, int direction, LayerCommands& cmds) {
        (void)selectedIndex;
        (void)direction;
        (void)cmds;
    }
    virtual const char* headerText() const = 0;
    virtual float headerFontScale() const { return 3.0f; }
    virtual float itemFontScale() const { return 1.8f; }
    virtual const char* hintText() const;
    virtual MenuLayout menuLayout() const { return MenuLayout::Vertical; }
    virtual void drawBackground(float winW, float winH) {
        (void)winW;
        (void)winH;
    }
    virtual void drawExtraUI(float winW, float winH) {
        (void)winW;
        (void)winH;
    }
    virtual void drawMenuHeader(float winW, float winH) const;
    virtual void drawMenuItems(float winW, float winH, const std::vector<MenuItem>& items) const;
    virtual void drawMenuHint(float winW, float winH, bool hasItems) const;

    int selectedIndex() const { return selectedIndex_; }
    void setSelectedIndex(int idx) { selectedIndex_ = idx; }

    SceneRenderer& renderer() { return renderer_; }
    VulkanRenderer& vulkan() { return vulkan_; }

   private:
    SceneRenderer& renderer_;
    VulkanRenderer& vulkan_;
    int selectedIndex_ = 0;
};

#pragma once
// =============================================================================
// hud_draw_list.h — + addBarFillSegmented に flat オプション (= kFlagBarFlat)
// =============================================================================

#include <cmath>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "renderer/hud_pipeline.h"

class HudDrawList {
   public:
    struct Shape {
        glm::vec2 min{0.f};
        glm::vec2 size{0.f};
        glm::vec4 color{1.f};
        HudPipeline::ShapeMode mode = HudPipeline::Rect;
        int32_t flags = 0;
        glm::vec4 extraParams{0.f};
    };

    void addRectFilled(const glm::vec2& min, const glm::vec2& size, const glm::vec4& color) {
        Shape s;
        s.min = min;
        s.size = size;
        s.color = color;
        s.mode = HudPipeline::Rect;
        shapes_.push_back(s);
    }

    void addRectOutline(const glm::vec2& min, const glm::vec2& size, const glm::vec4& color,
                        float thickness = 1.f) {
        addRectFilled(min, {size.x, thickness}, color);
        addRectFilled({min.x, min.y + size.y - thickness}, {size.x, thickness}, color);
        addRectFilled(min, {thickness, size.y}, color);
        addRectFilled({min.x + size.x - thickness, min.y}, {thickness, size.y}, color);
    }

    void addCircle(const glm::vec2& center, float radius, const glm::vec4& color,
                    bool gloss = false, float glossStrength = 0.6f) {
        Shape s;
        s.min = center - glm::vec2{radius};
        s.size = glm::vec2{radius * 2.f};
        s.color = color;
        s.mode = HudPipeline::Circle;
        s.flags = gloss ? HudPipeline::kFlagGloss : 0;
        s.extraParams = glm::vec4{radius, glossStrength, 0.f, 0.f};
        shapes_.push_back(s);
    }

    void addRing(const glm::vec2& center, float outerR, float innerR, const glm::vec4& color) {
        Shape s;
        s.min = center - glm::vec2{outerR};
        s.size = glm::vec2{outerR * 2.f};
        s.color = color;
        s.mode = HudPipeline::Ring;
        const float outerUV = 0.5f;
        const float innerUV = (outerR > 0.f) ? (innerR / outerR) * 0.5f : 0.f;
        s.extraParams = glm::vec4{outerUV, innerUV, 0.f, 0.f};
        shapes_.push_back(s);
    }

    void addCircleSegment(const glm::vec2& center, float outerR, float innerR,
                          float angleStart, float angleEnd, const glm::vec4& color) {
        Shape s;
        s.min = center - glm::vec2{outerR};
        s.size = glm::vec2{outerR * 2.f};
        s.color = color;
        s.mode = HudPipeline::CircleSegment;
        const float outerUV = 0.5f;
        const float innerUV = (outerR > 0.f) ? (innerR / outerR) * 0.5f : 0.f;
        s.extraParams = glm::vec4{outerUV, innerUV, angleStart, angleEnd};
        shapes_.push_back(s);
    }

    void addGradientRect(const glm::vec2& min, const glm::vec2& size, const glm::vec4& color,
                          float darken = 0.4f, bool vertical = true) {
        Shape s;
        s.min = min;
        s.size = size;
        s.color = color;
        s.mode = HudPipeline::GradientRect;
        s.extraParams = glm::vec4{0.f, 0.f, darken, vertical ? 1.f : 0.f};
        shapes_.push_back(s);
    }

    void addBeveledRect(const glm::vec2& min, const glm::vec2& size, const glm::vec4& color,
                         float bevelUV = 0.25f, float highlight = 0.35f, float shadow = 0.45f) {
        Shape s;
        s.min = min;
        s.size = size;
        s.color = color;
        s.mode = HudPipeline::BeveledRect;
        s.extraParams = glm::vec4{bevelUV, highlight, shadow, 0.f};
        shapes_.push_back(s);
    }

    void addMetalFrame(const glm::vec2& min, const glm::vec2& size, const glm::vec4& color,
                       float borderUV = 0.15f) {
        Shape s;
        s.min = min;
        s.size = size;
        s.color = color;
        s.mode = HudPipeline::MetalFrame;
        s.extraParams = glm::vec4{borderUV, 0.f, 0.f, 0.f};
        shapes_.push_back(s);
    }

    void addRivet(const glm::vec2& center, float radius, const glm::vec4& color) {
        Shape s;
        s.min = center - glm::vec2{radius};
        s.size = glm::vec2{radius * 2.f};
        s.color = color;
        s.mode = HudPipeline::Rivet;
        shapes_.push_back(s);
    }

    // セグメント分割 fill。
    // flat=true なら立体感 (グラデ + ハイライト) を無効化してベタ塗りに
    // (= kFlagBarFlat)。 鮮やかな色を確実に表示したい場合に使う。
    void addBarFillSegmented(const glm::vec2& min, const glm::vec2& size, const glm::vec4& color,
                              int segCount, int fullSegs, float tailFillRatio,
                              bool flat = false) {
        Shape s;
        s.min = min;
        s.size = size;
        s.color = color;
        s.mode = HudPipeline::BarFillSegmented;
        s.flags = flat ? HudPipeline::kFlagBarFlat : 0;
        s.extraParams = glm::vec4{
            static_cast<float>(segCount),
            tailFillRatio,
            static_cast<float>(fullSegs),
            0.f
        };
        shapes_.push_back(s);
    }

    void clear() { shapes_.clear(); }

    const std::vector<Shape>& shapes() const { return shapes_; }
    bool empty() const { return shapes_.empty(); }
    const std::vector<Shape>& rects() const { return shapes_; }

   private:
    std::vector<Shape> shapes_;
};

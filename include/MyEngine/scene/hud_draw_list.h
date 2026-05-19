#pragma once
// =============================================================================
// hud_draw_list.h — HUD 描画リスト (Rect/Circle/Ring/Segment/BarFlat)
// =============================================================================

#include <cstdint>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

struct HudInstance {
    glm::vec2 posTL{0.f};      // ピクセル左上
    glm::vec2 size{1.f, 1.f};
    glm::vec4 color{1.f};
    glm::vec4 extra{0.f};       // ring/segment 用 (x=innerRatio, y=startA, z=endA)
    uint32_t flags = 0;          // 0=Rect, 1=Circle, 2=Ring, 3=Segment, 4=BarFlat
};

class HudDrawList {
   public:
    void clear() { instances_.clear(); }

    void addRect(glm::vec2 posTL, glm::vec2 size, glm::vec4 color) {
        instances_.push_back({posTL, size, color, {}, 0});
    }

    void addBarFlat(glm::vec2 posTL, glm::vec2 size, glm::vec4 color) {
        instances_.push_back({posTL, size, color, {}, 4});
    }

    void addCircle(glm::vec2 center, float radius, glm::vec4 color) {
        const glm::vec2 size{radius * 2, radius * 2};
        instances_.push_back({center - radius, size, color, {}, 1});
    }

    void addRing(glm::vec2 center, float radiusOuter, float radiusInner, glm::vec4 color) {
        const glm::vec2 size{radiusOuter * 2, radiusOuter * 2};
        const float inner = radiusInner / radiusOuter;
        instances_.push_back({center - radiusOuter, size, color, glm::vec4{inner, 0, 0, 0}, 2});
    }

    void addSegment(glm::vec2 center, float radiusOuter, float radiusInner, float startA,
                     float endA, glm::vec4 color) {
        const glm::vec2 size{radiusOuter * 2, radiusOuter * 2};
        const float inner = radiusInner / radiusOuter;
        instances_.push_back(
            {center - radiusOuter, size, color, glm::vec4{inner, startA, endA, 0}, 3});
    }

    const std::vector<HudInstance>& instances() const { return instances_; }
    bool empty() const { return instances_.empty(); }

   private:
    std::vector<HudInstance> instances_;
};

#pragma once
// =============================================================================
// render_debug_system.h — F3 デバッグオーバーレイ (FPS / HP / 座標 / 入力)
// =============================================================================

#include <string>
#include <vector>

struct WorldData;
struct RuntimeState;

class RenderDebugSystem {
   public:
    enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };

    struct Line {
        std::string text;
        float r, g, b;
    };

    void update(const WorldData& wd, const RuntimeState& rt);

    const std::vector<Line>& lines() const { return lines_; }

   private:
    std::vector<Line> lines_;
};

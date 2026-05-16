#pragma once
// =============================================================================
// debug_line_renderer.h — デバッグ線描画 API (3D 弧 + 軸回転対応版)
// =============================================================================
// 役割:
//   各 Layer の buildScene() の中で「線・弧・扇形を積む」 ためのインターフェース。
//   Vulkan には依存しない (頂点を vector に積むだけ)。
//
// 主要 API:
//   低レベル:
//     addLine, addTriangle
//   水平面 (XZ) の扇形 (シンプル、 角度ベース):
//     addArcXZ, addCircleXZ, addSectorOutlineXZ, addSectorFilledXZ
//   任意 2 ベクトル間の弧 (短弧、 slerp):
//     addArcDir, addSectorOutlineDir, addSectorFilledDir
//   軸 + 符号付き角度 (方向と長弧を完全制御、 360°超え OK):
//     addArcAxis, addSectorOutlineAxis, addSectorFilledAxis
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "renderer/debug_line_vertex.h"

class DebugLineRenderer {
   public:
    void clear() {
        lineVertices_.clear();
        triVertices_.clear();
    }

    // ─── 低レベル API ──────────────────────────────────────────────
    void addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color) {
        lineVertices_.push_back({a, color});
        lineVertices_.push_back({b, color});
    }

    void addTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec4& color) {
        triVertices_.push_back({a, color});
        triVertices_.push_back({b, color});
        triVertices_.push_back({c, color});
    }

    // ─── 水平面 (XZ) の弧/扇形 API ──────────────────────────────────
    void addArcXZ(const glm::vec3& center, float radius, float startDeg, float endDeg,
                  const glm::vec4& color, int segments = 32) {
        if (segments < 1) segments = 1;
        const float stepDeg = (endDeg - startDeg) / static_cast<float>(segments);
        for (int i = 0; i < segments; ++i) {
            const float a = glm::radians(startDeg + stepDeg * i);
            const float b = glm::radians(startDeg + stepDeg * (i + 1));
            const glm::vec3 pa = center + glm::vec3{std::sin(a), 0.f, std::cos(a)} * radius;
            const glm::vec3 pb = center + glm::vec3{std::sin(b), 0.f, std::cos(b)} * radius;
            addLine(pa, pb, color);
        }
    }

    void addCircleXZ(const glm::vec3& center, float radius, const glm::vec4& color,
                     int segments = 64) {
        addArcXZ(center, radius, 0.f, 360.f, color, segments);
    }

    void addSectorOutlineXZ(const glm::vec3& center, float radius, float startDeg, float endDeg,
                            const glm::vec4& color, int segments = 32) {
        const float startRad = glm::radians(startDeg);
        const float endRad = glm::radians(endDeg);
        const glm::vec3 startEdge =
            center + glm::vec3{std::sin(startRad), 0.f, std::cos(startRad)} * radius;
        const glm::vec3 endEdge =
            center + glm::vec3{std::sin(endRad), 0.f, std::cos(endRad)} * radius;
        addLine(center, startEdge, color);
        addLine(center, endEdge, color);
        addArcXZ(center, radius, startDeg, endDeg, color, segments);
    }

    void addSectorFilledXZ(const glm::vec3& center, float radius, float startDeg, float endDeg,
                           const glm::vec4& color, int segments = 32) {
        if (segments < 1) segments = 1;
        const float stepDeg = (endDeg - startDeg) / static_cast<float>(segments);
        for (int i = 0; i < segments; ++i) {
            const float a = glm::radians(startDeg + stepDeg * i);
            const float b = glm::radians(startDeg + stepDeg * (i + 1));
            const glm::vec3 pa = center + glm::vec3{std::sin(a), 0.f, std::cos(a)} * radius;
            const glm::vec3 pb = center + glm::vec3{std::sin(b), 0.f, std::cos(b)} * radius;
            addTriangle(center, pa, pb, color);
        }
    }

    // ─── 任意 2 ベクトル間の弧 (短弧、 slerp) ───────────────────────
    void addArcDir(const glm::vec3& center, const glm::vec3& startDir, const glm::vec3& endDir,
                   float radius, const glm::vec4& color, int segments = 32) {
        if (segments < 1) segments = 1;
        glm::vec3 prevPoint = center + startDir * radius;
        for (int i = 1; i <= segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const glm::vec3 curDir = slerpUnit(startDir, endDir, t);
            const glm::vec3 curPoint = center + curDir * radius;
            addLine(prevPoint, curPoint, color);
            prevPoint = curPoint;
        }
    }

    void addSectorOutlineDir(const glm::vec3& center, const glm::vec3& startDir,
                             const glm::vec3& endDir, float radius, const glm::vec4& color,
                             int segments = 32) {
        const glm::vec3 startEdge = center + startDir * radius;
        const glm::vec3 endEdge = center + endDir * radius;
        addLine(center, startEdge, color);
        addLine(center, endEdge, color);
        addArcDir(center, startDir, endDir, radius, color, segments);
    }

    void addSectorFilledDir(const glm::vec3& center, const glm::vec3& startDir,
                            const glm::vec3& endDir, float radius, const glm::vec4& color,
                            int segments = 32) {
        if (segments < 1) segments = 1;
        glm::vec3 prevPoint = center + startDir * radius;
        for (int i = 1; i <= segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const glm::vec3 curDir = slerpUnit(startDir, endDir, t);
            const glm::vec3 curPoint = center + curDir * radius;
            addTriangle(center, prevPoint, curPoint, color);
            prevPoint = curPoint;
        }
    }

    // ─── 軸 + 符号付き角度ベース (方向と長弧を完全制御、 360° 超え OK) ─
    // startDir を rotationAxis 回りに sweepAngleDeg 回転させた軌跡。
    // 符号で回転方向、 360°超えで複数周も表現可能。
    // segments は弧の分解数 (大きい sweep ほど多めが綺麗)。
    void addArcAxis(const glm::vec3& center, const glm::vec3& startDir,
                    const glm::vec3& rotationAxis, float sweepAngleDeg, float radius,
                    const glm::vec4& color, int segments = 32) {
        if (segments < 1) segments = 1;
        glm::vec3 prevPoint = center + startDir * radius;
        for (int i = 1; i <= segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float angleRad = glm::radians(sweepAngleDeg) * t;
            const glm::quat q = glm::angleAxis(angleRad, rotationAxis);
            const glm::vec3 curDir = q * startDir;
            const glm::vec3 curPoint = center + curDir * radius;
            addLine(prevPoint, curPoint, color);
            prevPoint = curPoint;
        }
    }

    void addSectorOutlineAxis(const glm::vec3& center, const glm::vec3& startDir,
                              const glm::vec3& rotationAxis, float sweepAngleDeg, float radius,
                              const glm::vec4& color, int segments = 32) {
        const glm::vec3 startEdge = center + startDir * radius;
        // 終点を計算
        const float fullAngleRad = glm::radians(sweepAngleDeg);
        const glm::quat fullQ = glm::angleAxis(fullAngleRad, rotationAxis);
        const glm::vec3 endDir = fullQ * startDir;
        const glm::vec3 endEdge = center + endDir * radius;

        addLine(center, startEdge, color);
        addLine(center, endEdge, color);
        addArcAxis(center, startDir, rotationAxis, sweepAngleDeg, radius, color, segments);
    }

    void addSectorFilledAxis(const glm::vec3& center, const glm::vec3& startDir,
                             const glm::vec3& rotationAxis, float sweepAngleDeg, float radius,
                             const glm::vec4& color, int segments = 32) {
        if (segments < 1) segments = 1;
        glm::vec3 prevPoint = center + startDir * radius;
        for (int i = 1; i <= segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float angleRad = glm::radians(sweepAngleDeg) * t;
            const glm::quat q = glm::angleAxis(angleRad, rotationAxis);
            const glm::vec3 curDir = q * startDir;
            const glm::vec3 curPoint = center + curDir * radius;
            addTriangle(center, prevPoint, curPoint, color);
            prevPoint = curPoint;
        }
    }

    // ─── アクセサ ───────────────────────────────────────────────────
    const std::vector<DebugLineVertex>& lineVertices() const { return lineVertices_; }
    const std::vector<DebugLineVertex>& triVertices() const { return triVertices_; }

   private:
    std::vector<DebugLineVertex> lineVertices_;
    std::vector<DebugLineVertex> triVertices_;

    static glm::vec3 slerpUnit(const glm::vec3& a, const glm::vec3& b, float t) {
        const float d = glm::clamp(glm::dot(a, b), -1.f, 1.f);
        const float theta = std::acos(d);
        if (theta < 1e-4f) {
            return glm::normalize(glm::mix(a, b, t));
        }
        const float sinTheta = std::sin(theta);
        const float w1 = std::sin((1.f - t) * theta) / sinTheta;
        const float w2 = std::sin(t * theta) / sinTheta;
        return a * w1 + b * w2;
    }
};

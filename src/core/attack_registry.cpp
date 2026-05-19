// =============================================================================
// attack_registry.cpp — 攻撃モーションの定義 (回転軸 + 角度ベース版)
// =============================================================================
// 各 def は「始点方向 + 回転軸 + 符号付き回転角度」 で表現される。
// 右ねじ規則:
//   axis=(0,1,0) (Y上向き) で +角度 = 上から見て反時計回り
//   axis=(1,0,0) (X右向き) で +角度 = 右から見て反時計回り
// =============================================================================
#include "core/attack_registry.h"

#include <cmath>
#include <iostream>

namespace attack_registry {

namespace {

constexpr float kDeg2Rad = 3.14159265358979323846f / 180.f;

// 水平方向の単位ベクトル (yaw 度から、 XZ 平面)
inline glm::vec3 horizDir(float yawDeg) {
    const float r = yawDeg * kDeg2Rad;
    return glm::vec3{std::sin(r), 0.f, std::cos(r)};
}

// 垂直方向の単位ベクトル (pitch 度から、 YZ 平面)
inline glm::vec3 vertDir(float pitchDeg) {
    const float r = pitchDeg * kDeg2Rad;
    return glm::vec3{0.f, std::sin(r), std::cos(r)};
}

struct DefsTable {
    AttackDef slash;
    AttackDef smash;
    AttackDef smashDown;

    DefsTable() {
        // ─── Slash: 横なぎ (LMB) ──────────────────────────────
        // 始点: 右 75° の方向、 Y 軸回りに +150° 回転 = 右→正面→左 (反時計回り)
        slash = {
            AttackKind::Slash,
            "Slash",
            AnimState::Slash,
            /* windupTime         */ 0.10f,
            /* activeTime         */ 0.15f,
            /* recoveryTime       */ 0.25f,
            /* startDir           */ horizDir(-75.f),   // 右 75°
            /* rotationAxis       */ glm::vec3{0.f, 1.f, 0.f},
            /* sweepAngleDeg      */ +150.f,            // 右→左 (反時計回り = Y軸右ねじ正)
            /* range              */ 2.4f,
            /* halfWidthDeg       */ 25.f,
            /* damage             */ 1,
            /* lockMovementGround */ true,
            /* canCancelOnLand    */ true,
        };

        // ─── Smash: 振り下ろし (RMB、 地上) ───────────────────
        // 始点: 頭上後ろ (pitch +105°)、 X 軸回りに -105° 回転 = 前方水平へ
        // X 軸右ねじで負方向 = 右から見て時計回り = 「上→前→下」 方向
        smash = {
            AttackKind::Smash,
            "Smash",
            AnimState::Smash,
            /* windupTime         */ 0.20f,
            /* activeTime         */ 0.12f,
            /* recoveryTime       */ 0.30f,
            /* startDir           */ vertDir(+105.f),   // 頭上より少し後ろ
            /* rotationAxis       */ glm::vec3{1.f, 0.f, 0.f},
            /* sweepAngleDeg      */ -105.f,            // 振り下ろし方向
            /* range              */ 2.6f,
            /* halfWidthDeg       */ 30.f,
            /* damage             */ 2,
            /* lockMovementGround */ true,
            /* canCancelOnLand    */ false,
        };

        // ─── SmashDown: 空中急降下叩きつけ ────────────────────
        // 着地時に周囲 360° 衝撃波を出すので、 active 中の弧スイープは無効化される
        // (CombatSystem の isDiving 中の hit スキップ)。
        // def としては「Y 軸回りに +360° 全周」 として一応定義 (デバッグ可視化用)。
        smashDown = {
            AttackKind::SmashDown,
            "SmashDown",
            AnimState::SmashDown,
            /* windupTime         */ 0.10f,
            /* activeTime         */ 0.15f,
            /* recoveryTime       */ 0.35f,
            /* startDir           */ horizDir(0.f),     // 前方
            /* rotationAxis       */ glm::vec3{0.f, 1.f, 0.f},
            /* sweepAngleDeg      */ +360.f,            // 全周
            /* range              */ 3.0f,
            /* halfWidthDeg       */ 180.f,
            /* damage             */ 3,
            /* lockMovementGround */ true,
            /* canCancelOnLand    */ false,
        };
    }
};

const DefsTable& defs() {
    static DefsTable t;
    return t;
}

}  // namespace

const AttackDef& get(AttackKind kind) {
    const auto& t = defs();
    switch (kind) {
        case AttackKind::Slash:     return t.slash;
        case AttackKind::Smash:     return t.smash;
        case AttackKind::SmashDown: return t.smashDown;
    }
    std::cerr << "[AttackRegistry] WARNING: AttackKind not found (idx="
              << static_cast<int>(kind) << "), falling back to Slash\n";
    return t.slash;
}

}  // namespace attack_registry

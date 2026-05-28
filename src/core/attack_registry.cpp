// =============================================================================
// attack_registry.cpp - Attack motion definitions (rotation axis + angle based)
// =============================================================================
// Each def is expressed as "start direction + rotation axis + signed rotation angle".
// Right-hand rule:
//   axis=(0,1,0) (Y up):    +angle = counter-clockwise when viewed from above
//   axis=(1,0,0) (X right): +angle = counter-clockwise when viewed from the right
// =============================================================================
#include "core/attack_registry.h"

#include <cmath>
#include <iostream>

namespace attack_registry {

namespace {

constexpr float kDeg2Rad = 3.14159265358979323846f / 180.f;

// Unit vector in horizontal direction (from yaw degrees, XZ plane)
inline glm::vec3 horizDir(float yawDeg) {
    const float r = yawDeg * kDeg2Rad;
    return glm::vec3{std::sin(r), 0.f, std::cos(r)};
}

// Unit vector in vertical direction (from pitch degrees, YZ plane)
inline glm::vec3 vertDir(float pitchDeg) {
    const float r = pitchDeg * kDeg2Rad;
    return glm::vec3{0.f, std::sin(r), std::cos(r)};
}

struct DefsTable {
    AttackDef slash;
    AttackDef smash;
    AttackDef smashDown;

    DefsTable() {
        // --- Slash: horizontal swing (LMB) ---
        // Start: right 75 deg direction, rotate +150 deg around Y = right -> front -> left (CCW)
        slash = {
            AttackKind::Slash,
            "Slash",
            AnimState::Slash,
            /* windupTime         */ 0.10f,
            /* activeTime         */ 0.15f,
            /* recoveryTime       */ 0.25f,
            /* startDir           */ horizDir(-75.f),  // right 75 deg
            /* rotationAxis       */ glm::vec3{0.f, 1.f, 0.f},
            /* sweepAngleDeg      */ +150.f,  // right -> left (CCW = Y right-hand positive)
            /* range              */ 2.4f,
            /* halfWidthDeg       */ 25.f,
            /* damage             */ 1,
            /* lockMovementGround */ true,
            /* canCancelOnLand    */ true,
        };

        // --- Smash: downward swing (RMB, grounded) ---
        // Start: overhead-back (pitch +105 deg), rotate -105 deg around X = toward front horizontal
        // Right-hand rule on X: negative = clockwise viewed from +X side = "top -> front -> bottom" motion
        smash = {
            AttackKind::Smash,
            "Smash",
            AnimState::Smash,
            /* windupTime         */ 0.20f,
            /* activeTime         */ 0.12f,
            /* recoveryTime       */ 0.30f,
            /* startDir           */ vertDir(+105.f),  // slightly behind overhead
            /* rotationAxis       */ glm::vec3{1.f, 0.f, 0.f},
            /* sweepAngleDeg      */ 105.f,  // downward swing direction
            /* range              */ 2.6f,
            /* halfWidthDeg       */ 30.f,
            /* damage             */ 2,
            /* lockMovementGround */ true,
            /* canCancelOnLand    */ false,
        };

        // --- SmashDown: aerial dive slam ---
        // Emits a 360 deg shockwave on landing, so the arc sweep during active is disabled
        // (CombatSystem skips hit while isDiving).
        // As a def, still defined as "+360 deg full circle around Y" for debug visualization.
        smashDown = {
            AttackKind::SmashDown,
            "SmashDown",
            AnimState::SmashDown,
            /* windupTime         */ 0.10f,
            /* activeTime         */ 0.15f,
            /* recoveryTime       */ 0.35f,
            /* startDir           */ horizDir(0.f),  // forward
            /* rotationAxis       */ glm::vec3{0.f, 1.f, 0.f},
            /* sweepAngleDeg      */ +360.f,  // full circle
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
        case AttackKind::Slash:
            return t.slash;
        case AttackKind::Smash:
            return t.smash;
        case AttackKind::SmashDown:
            return t.smashDown;
    }
    std::cerr << "[AttackRegistry] WARNING: AttackKind not found (idx=" << static_cast<int>(kind)
              << "), falling back to Slash\n";
    return t.slash;
}

}  // namespace attack_registry

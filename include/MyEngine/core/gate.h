#pragma once
// =============================================================================
// core/gate.h — + 鍵チェック
// =============================================================================
// requiresKey:
//   false (default): 誰でも開ける
//   true            : requiredKey を持ってないと開かない
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "core/key.h"

struct CGate {
    enum class State { Closed, Opening, Open };
    enum class OpenMode { Slide, Rotate };

    State state = State::Closed;
    OpenMode openMode = OpenMode::Slide;

    glm::vec3 closedPos{0.f};
    float duration = 1.0f;
    float progress = 0.f;
    float interactRange = 3.0f;
    int groupId = 0;

    glm::vec3 openOffset{0.f};

    glm::vec3 hingeOffsetLocal{0.f};
    float closedYaw = 0.f;
    float openYawDelta = 75.f;

    // 鍵チェック
    bool requiresKey = false;
    KeyType requiredKey = KeyType::Gold;

    bool isOpenable() const { return state == State::Closed; }
};

struct GateTag {};

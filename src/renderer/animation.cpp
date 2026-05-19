// src/renderer/animation.cpp
#include "renderer/animation.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace {

template <typename KeyArray>
inline size_t findKeyIndex(const KeyArray& keys, float t, size_t& lastIdx) {
    const size_t n = keys.size();
    if (n <= 1) return 0;

    if (lastIdx >= n - 1) {
        lastIdx = 0;
    }

    if (t >= keys[lastIdx].time) {
        while (lastIdx + 1 < n && keys[lastIdx + 1].time <= t) {
            lastIdx++;
        }
        if (lastIdx >= n - 1) lastIdx = n - 2;
        return lastIdx;
    }

    lastIdx = 0;
    while (lastIdx + 1 < n && keys[lastIdx + 1].time <= t) {
        lastIdx++;
    }
    if (lastIdx >= n - 1) lastIdx = n - 2;
    return lastIdx;
}

inline float lerpFactor(float t0, float t1, float t) {
    const float dt = t1 - t0;
    if (dt <= 1e-6f) return 0.f;
    float a = (t - t0) / dt;
    if (a < 0.f) a = 0.f;
    else if (a > 1.f) a = 1.f;
    return a;
}

}  // namespace

// =============================================================================
// Phase 3-F: TRS をサンプリング (クロスフェード補間で使用)
// =============================================================================
LocalTRS AnimationChannel::sampleLocalTRS(float t) const {
    LocalTRS out;

    if (!positionKeys.empty()) {
        if (positionKeys.size() == 1) {
            out.pos = positionKeys[0].value;
        } else {
            const size_t i0 = findKeyIndex(positionKeys, t, lastPosIdx);
            const size_t i1 = i0 + 1;
            const float a = lerpFactor(positionKeys[i0].time, positionKeys[i1].time, t);
            out.pos = glm::mix(positionKeys[i0].value, positionKeys[i1].value, a);
        }
    }

    if (!rotationKeys.empty()) {
        if (rotationKeys.size() == 1) {
            out.rot = rotationKeys[0].value;
        } else {
            const size_t i0 = findKeyIndex(rotationKeys, t, lastRotIdx);
            const size_t i1 = i0 + 1;
            const float a = lerpFactor(rotationKeys[i0].time, rotationKeys[i1].time, t);
            out.rot = glm::slerp(rotationKeys[i0].value, rotationKeys[i1].value, a);
        }
    }

    if (!scaleKeys.empty()) {
        if (scaleKeys.size() == 1) {
            out.scale = scaleKeys[0].value;
        } else {
            const size_t i0 = findKeyIndex(scaleKeys, t, lastScaleIdx);
            const size_t i1 = i0 + 1;
            const float a = lerpFactor(scaleKeys[i0].time, scaleKeys[i1].time, t);
            out.scale = glm::mix(scaleKeys[i0].value, scaleKeys[i1].value, a);
        }
    }

    return out;
}

// 既存 API: TRS を取得して合成 matrix を返す
glm::mat4 AnimationChannel::sampleLocalMatrix(float t) const {
    return sampleLocalTRS(t).toMatrix();
}

float AnimationClip::wrapTime(float t) const {
    if (duration <= 0.f) return 0.f;
    float wrapped = std::fmod(t, duration);
    if (wrapped < 0.f) wrapped += duration;
    return wrapped;
}

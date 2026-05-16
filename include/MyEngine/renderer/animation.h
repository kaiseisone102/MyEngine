// include/MyEngine/renderer/animation.h
#pragma once
// =============================================================================
// animation.h — Phase 3-F (クロスフェード補間対応)
// =============================================================================
// 段階F 追加:
//   LocalTRS: 位置/回転/スケールの分離形 (Translation, Rotation, Scale)
//   AnimationChannel::sampleLocalTRS(t): TRSを返す API
//
// クロスフェード補間時、2つのアニメの TRS を取得して個別に補間してから
// 合成 matrix にする。matrix 単位で lerp すると非自然な変形になるため、
// 必ず TRS 段階で補間する。
//
// 既存の sampleLocalMatrix(t) は維持 (内部で sampleLocalTRS を呼び合成)。
// =============================================================================

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

class Skeleton;

struct PositionKey {
    float time = 0.f;
    glm::vec3 value{0.f};
};

struct RotationKey {
    float time = 0.f;
    glm::quat value{1.f, 0.f, 0.f, 0.f};
};

struct ScaleKey {
    float time = 0.f;
    glm::vec3 value{1.f};
};

// Phase 3-F: 補間用の分離形 (Translation, Rotation, Scale)
struct LocalTRS {
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 scale{1.f};

    // TRS を 4x4 matrix に合成する (rotation × scale × translation の通常順)。
    glm::mat4 toMatrix() const {
        glm::mat4 m = glm::mat4_cast(rot);
        m[0] *= scale.x;
        m[1] *= scale.y;
        m[2] *= scale.z;
        m[3] = glm::vec4(pos, 1.f);
        return m;
    }

    // 2つの TRS を補間する。位置・スケールは lerp、回転は slerp。
    static LocalTRS blend(const LocalTRS& from, const LocalTRS& to, float alpha) {
        LocalTRS out;
        out.pos = glm::mix(from.pos, to.pos, alpha);
        out.rot = glm::slerp(from.rot, to.rot, alpha);
        out.scale = glm::mix(from.scale, to.scale, alpha);
        return out;
    }
};

struct AnimationChannel {
    std::string boneName;

    std::vector<PositionKey> positionKeys;
    std::vector<RotationKey> rotationKeys;
    std::vector<ScaleKey> scaleKeys;

    // sample() 用キャッシュ。
    mutable size_t lastPosIdx = 0;
    mutable size_t lastRotIdx = 0;
    mutable size_t lastScaleIdx = 0;

    // 既存 API: TRSをサンプリング -> 合成 matrix を返す
    glm::mat4 sampleLocalMatrix(float t) const;

    // Phase 3-F: TRS のままサンプリング (クロスフェード補間用)
    LocalTRS sampleLocalTRS(float t) const;
};

class AnimationClip {
   public:
    std::string name;
    float duration = 0.f;
    float ticksPerSecond = 25.f;
    std::vector<AnimationChannel> channels;

    float wrapTime(float t) const;
};

struct AnimationBinding {
    const AnimationClip* clip = nullptr;
    std::vector<int> boneToChannel;

    bool valid() const { return clip != nullptr && !boneToChannel.empty(); }
};

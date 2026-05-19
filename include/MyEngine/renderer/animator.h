// include/MyEngine/renderer/animator.h
#pragma once
// =============================================================================
// animator.h — Phase 5-B
// =============================================================================
// Phase 3-F: クロスフェード補間 (from_ / to_ の 2 系統バインディング)
// Phase 5-B: boneWorldTransform API 追加 (装備品の attach 用)
// =============================================================================

#include <glm/glm.hpp>
#include <vector>

#include "renderer/animation.h"

class Skeleton;

class Animator {
   public:
    static constexpr float kDefaultBlendDuration = 0.15f;

    void bind(const Skeleton* skeleton, const AnimationClip* clip);
    void setClip(const AnimationClip* clip);
    void setClip(const AnimationClip* clip, float blendDuration);

    void update(float dt);
    void setTime(float t);

    void computeSkinMatrices(std::vector<glm::mat4>& out) const;

    int boneCount() const { return boneCount_; }
    bool ready() const { return skeleton_ != nullptr; }
    float currentTime() const { return toTime_; }
    const AnimationClip* currentClip() const { return to_.clip; }
    bool isBlending() const { return blending_; }

    // =========================================================================
    // Phase 5-B: 指定ボーンの「モデルローカル空間でのグローバル変換」を取得
    // =========================================================================
    // 装備品 (盾・剣など) を特定ボーンに attach するために使用する。
    //
    // 戻り値の意味:
    //   - 戻り値 = ルートからその bone までの累積 local 変換 (= bone の現在のポーズ)
    //   - boneIndex 範囲外の場合は単位行列を返す
    //
    // 使用上の注意:
    //   - computeSkinMatrices() 後に呼び出すこと (それより前は古い値)
    //   - 「ワールド空間」での位置を得るには、CTransform の matrix() を掛ける必要がある:
    //       worldTransform = playerCTransform.matrix() * animator.boneWorldTransform(idx)
    glm::mat4 boneWorldTransform(int boneIndex) const {
        if (boneIndex < 0 || boneIndex >= static_cast<int>(globalTransforms_.size())) {
            return glm::mat4(1.f);
        }
        return globalTransforms_[boneIndex];
    }

   private:
    const Skeleton* skeleton_ = nullptr;
    int boneCount_ = 0;

    AnimationBinding from_;
    AnimationBinding to_;
    float fromTime_ = 0.f;
    float toTime_ = 0.f;

    bool blending_ = false;
    float blendAlpha_ = 1.f;
    float blendDuration_ = kDefaultBlendDuration;

    mutable std::vector<glm::mat4> globalTransforms_;

    AnimationBinding makeBinding(const AnimationClip* clip) const;
};

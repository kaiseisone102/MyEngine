// src/renderer/animator.cpp
#include "renderer/animator.h"

#include <algorithm>
#include <iostream>

#include "renderer/animation.h"
#include "renderer/skeleton.h"

void Animator::bind(const Skeleton* skeleton, const AnimationClip* clip) {
    skeleton_ = skeleton;
    boneCount_ = skeleton ? skeleton->boneCount() : 0;
    fromTime_ = 0.f;
    toTime_ = 0.f;
    blending_ = false;
    blendAlpha_ = 1.f;
    globalTransforms_.assign(boneCount_, glm::mat4(1.f));

    from_ = {};
    to_ = makeBinding(clip);
}

void Animator::setClip(const AnimationClip* clip) {
    setClip(clip, kDefaultBlendDuration);
}

void Animator::setClip(const AnimationClip* clip, float blendDuration) {
    if (!skeleton_) return;
    if (to_.clip == clip) {
        // 同じクリップへの切替要求 → 何もしない
        return;
    }

    // 即時切替の場合
    if (blendDuration <= 0.f) {
        from_ = {};
        to_ = makeBinding(clip);
        toTime_ = 0.f;
        fromTime_ = 0.f;
        blending_ = false;
        blendAlpha_ = 1.f;
        return;
    }

    // クロスフェード開始:
    //   現在の to_ を from_ にスナップショット (現ポーズ = フェードアウト元)
    //   新しいクリップを to_ に bind
    //
    // 注意: ブレンド中に再度 setClip された場合、現在の "中間ポーズ" を
    //       完璧に保存するには 3 系統が必要。実用的な妥協として「現在の to_ を
    //       そのまま from_ に」する。多少の不自然さは出るがブレンド時間が短い
    //       (0.15秒) ので体感上問題なし。
    from_ = to_;
    fromTime_ = toTime_;

    to_ = makeBinding(clip);
    toTime_ = 0.f;

    blending_ = true;
    blendAlpha_ = 0.f;
    blendDuration_ = blendDuration;
}

AnimationBinding Animator::makeBinding(const AnimationClip* clip) const {
    AnimationBinding b;
    if (!skeleton_ || !clip) return b;

    b.clip = clip;
    b.boneToChannel.assign(boneCount_, -1);

    int resolved = 0;
    int unresolved = 0;
    for (size_t c = 0; c < clip->channels.size(); ++c) {
        const int boneIdx = skeleton_->findBoneByName(clip->channels[c].boneName);
        if (boneIdx >= 0 && boneIdx < boneCount_) {
            b.boneToChannel[boneIdx] = static_cast<int>(c);
            ++resolved;
        } else {
            ++unresolved;
        }
    }

    std::cout << "[Animator] bound clip='" << clip->name << "': " << resolved
              << " channels resolved, " << unresolved << " unresolved\n";

    return b;
}

void Animator::update(float dt) {
    // to_ は常に進行
    if (to_.valid()) {
        const AnimationClip* clip = to_.clip;
        if (clip->duration > 0.f) {
            toTime_ += dt;
            if (toTime_ >= clip->duration) {
                toTime_ = clip->wrapTime(toTime_);
            }
        }
    }

    // ブレンド中は from_ も進行 (フェードアウト中もアニメは流す)
    if (blending_ && from_.valid()) {
        const AnimationClip* clip = from_.clip;
        if (clip->duration > 0.f) {
            fromTime_ += dt;
            if (fromTime_ >= clip->duration) {
                fromTime_ = clip->wrapTime(fromTime_);
            }
        }

        // ブレンド進行
        if (blendDuration_ > 0.f) {
            blendAlpha_ += dt / blendDuration_;
            if (blendAlpha_ >= 1.f) {
                blendAlpha_ = 1.f;
                blending_ = false;
                from_ = {};  // フェードアウト完了 → 解放
            }
        } else {
            blendAlpha_ = 1.f;
            blending_ = false;
            from_ = {};
        }
    }
}

void Animator::setTime(float t) {
    if (to_.valid()) {
        toTime_ = to_.clip->wrapTime(t);
    } else {
        toTime_ = 0.f;
    }
}

void Animator::computeSkinMatrices(std::vector<glm::mat4>& out) const {
    if (!skeleton_ || boneCount_ == 0) {
        out.clear();
        return;
    }

    if (out.size() != static_cast<size_t>(boneCount_)) {
        out.resize(boneCount_);
    }
    if (globalTransforms_.size() != static_cast<size_t>(boneCount_)) {
        globalTransforms_.resize(boneCount_);
    }

    const auto& bones = skeleton_->bones();
    const glm::mat4& rootParent = skeleton_->globalInverseTransform();

    // ブレンド中かどうかで処理を分岐 (定常時は1経路で軽量)
    if (blending_ && from_.valid() && to_.valid()) {
        // 2 経路 + TRS 補間
        const AnimationClip& fromClip = *from_.clip;
        const AnimationClip& toClip = *to_.clip;
        const float alpha = blendAlpha_;

        for (int i = 0; i < boneCount_; ++i) {
            const Bone& b = bones[i];

            const int fromCh = from_.boneToChannel[i];
            const int toCh = to_.boneToChannel[i];

            glm::mat4 local;

            if (fromCh >= 0 && toCh >= 0) {
                // 両方のチャネルあり: TRS で補間
                const LocalTRS trsFrom =
                    fromClip.channels[fromCh].sampleLocalTRS(fromTime_);
                const LocalTRS trsTo = toClip.channels[toCh].sampleLocalTRS(toTime_);
                local = LocalTRS::blend(trsFrom, trsTo, alpha).toMatrix();
            } else if (toCh >= 0) {
                // to のみあり (from にこのチャネルなし) → from は bind pose 扱い
                local = toClip.channels[toCh].sampleLocalMatrix(toTime_);
            } else if (fromCh >= 0) {
                // from のみあり → from を使用 (フェードアウト中)
                local = fromClip.channels[fromCh].sampleLocalMatrix(fromTime_);
            } else {
                // 両方なし → bind pose
                local = b.localBindTransform;
            }

            const glm::mat4& parentGlobal =
                (b.parentIndex >= 0) ? globalTransforms_[b.parentIndex] : rootParent;
            globalTransforms_[i] = parentGlobal * local;
            out[i] = globalTransforms_[i] * b.inverseBindMatrix;
        }
    } else if (to_.valid()) {
        // 定常状態: to_ のみサンプリング (1経路、軽量)
        const AnimationClip& clip = *to_.clip;
        for (int i = 0; i < boneCount_; ++i) {
            const Bone& b = bones[i];
            glm::mat4 local;
            const int ch = to_.boneToChannel[i];
            if (ch >= 0) {
                local = clip.channels[ch].sampleLocalMatrix(toTime_);
            } else {
                local = b.localBindTransform;
            }
            const glm::mat4& parentGlobal =
                (b.parentIndex >= 0) ? globalTransforms_[b.parentIndex] : rootParent;
            globalTransforms_[i] = parentGlobal * local;
            out[i] = globalTransforms_[i] * b.inverseBindMatrix;
        }
    } else {
        // クリップ未設定 → bind pose
        for (int i = 0; i < boneCount_; ++i) {
            const Bone& b = bones[i];
            const glm::mat4& parentGlobal =
                (b.parentIndex >= 0) ? globalTransforms_[b.parentIndex] : rootParent;
            globalTransforms_[i] = parentGlobal * b.localBindTransform;
            out[i] = globalTransforms_[i] * b.inverseBindMatrix;
        }
    }
}

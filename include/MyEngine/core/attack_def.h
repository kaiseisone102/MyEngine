#pragma once
// =============================================================================
// attack_def.h — 攻撃モーション定義 (回転軸 + 角度ベース版)
// =============================================================================
// 設計:
//   攻撃の判定軌跡を「始点ベクトル + 回転軸 + 回転角度 (符号付き)」 で表現する。
//   - startDir:       active 開始時の刃の方向 (単位ベクトル、 ローカル空間)
//   - rotationAxis:   回転軸 (単位ベクトル、 ローカル空間、 右ねじ規則で方向決定)
//   - sweepAngleDeg:  回転角度 (符号付き、 度)。
//                       +で軸の右ねじ正方向、 -で逆方向。
//                       360°超え (= 1 回転以上) も可能。
//
// ローカル空間:
//   +Z = player 前方、 +Y = 上、 +X = 右
//
// 右ねじ規則の例:
//   axis = (0, 1, 0)  (Y 軸上向き)
//     +角度 = 上から見て反時計回り
//     例: startDir = horizDir(-75°) (右後ろ), sweep = +150°
//         → 右後ろ → 正面 → 左後ろ (横なぎ)
//
//   axis = (1, 0, 0)  (X 軸右向き)
//     +角度 = 右から見て反時計回り
//     垂直面で 「前 → 上 → 後ろ → 下 → 前」 と回る
//     例: startDir = vertDir(+105°) (頭上後ろ), sweep = -105°
//         → 頭上後ろ → 頭上 → 前方水平 (振り下ろし)
//
//   axis = (0, 1, 0), sweep = +360° → 全周スピン
//
// メリット:
//   - 方向が符号で完全に明示 (±で逆回り)
//   - 360°超えの長弧 (SpinAttack 等) を表現可能
//   - 斜め軸の指定で袈裟切り等も自然
//   - dirAt(t) で各時刻のローカル方向が一発計算
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "core/anim_state.h"

enum class AttackKind {
    Slash,        // 横なぎ (LMB)
    Smash,        // 振り下ろし (RMB、 地上)
    SmashDown,    // 急降下叩きつけ (RMB、 空中)
};

enum class AttackPhase {
    Idle,
    Windup,
    Active,
    Recovery,
};

struct AttackDef {
    AttackKind kind;
    const char* name;
    AnimState animState;

    // ─── タイミング (秒) ─────────────────────────────────────
    float windupTime;
    float activeTime;
    float recoveryTime;

    // ─── スイープ軌跡 (回転表現) ─────────────────────────────
    glm::vec3 startDir;        // 始点 (単位ベクトル、 ローカル空間)
    glm::vec3 rotationAxis;    // 回転軸 (単位ベクトル、 右ねじ規則)
    float     sweepAngleDeg;   // 回転角度 (符号付き)

    // ─── 判定パラメータ ──────────────────────────────────────
    float range;
    float halfWidthDeg;
    int   damage;

    // ─── 挙動 ────────────────────────────────────────────────
    bool lockMovementGround;
    bool canCancelOnLand;

    // ─── ヘルパー ────────────────────────────────────────────
    float totalDuration() const { return windupTime + activeTime + recoveryTime; }
    float windupEnd()    const { return windupTime; }
    float activeStart()  const { return windupTime; }
    float activeEnd()    const { return windupTime + activeTime; }

    // 時刻 t (0.0〜1.0) でのローカル空間方向ベクトル。
    // t=0 で startDir、 t=1 で startDir を rotationAxis 回りに sweepAngleDeg 回転した結果。
    glm::vec3 dirAt(float t) const {
        const float angleRad = glm::radians(sweepAngleDeg) * t;
        const glm::quat q = glm::angleAxis(angleRad, rotationAxis);
        return q * startDir;
    }

    // 終点 (t=1) でのローカル方向 (デバッグ可視化等で使う)
    glm::vec3 endDir() const { return dirAt(1.f); }
};

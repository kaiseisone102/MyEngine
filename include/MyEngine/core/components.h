#pragma once
// =============================================================================
// components.h — + CHealth に displayedTotalHp / damageDelayTimer / healFlashTimer
// =============================================================================

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <flecs.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "core/anim_state.h"
#include "core/grip.h"
#include "core/particle.h"
#include "renderer/animator.h"
#include "renderer/skin_buffer_pool.h"
#include "world/stage_id.h"

class Model;

struct CTransform {
    glm::vec3 pos = {0.f, 0.f, 0.f};
    float yaw = 0.f;
    glm::vec3 scale = {1.f, 1.f, 1.f};

    glm::mat4 matrix() const {
        glm::mat4 m{1.f};
        m = glm::translate(m, pos);
        m = glm::rotate(m, glm::radians(yaw), {0.f, 1.f, 0.f});
        m = glm::scale(m, scale);
        return m;
    }
};

struct CVelocity {
    float y = 0.f;
    glm::vec2 xz{0.f};
};

struct CPhysics {
    bool onGround = false;
    bool jumpReq = false;
    float speed = 5.f;
    int jumpsRemaining = 2;
    int maxJumps = 2;
    bool usedDoubleJump = false;

    flecs::entity standingOn;
};

enum class AttackType { Slash, Smash, SmashDown, SpinAttack };

inline bool isStrongAttack(AttackType t) {
    return t == AttackType::Smash || t == AttackType::SmashDown || t == AttackType::SpinAttack;
}

struct AttackDef;

struct CAttack {
    const AttackDef* def = nullptr;
    float elapsed = 0.f;

    bool isAerial = false;
    bool isDiving = false;

    float diveLiftStartY = 0.f;
    float diveLiftHeight = 0.f;
    bool diveDropStarted = false;

    std::vector<flecs::entity> hitEntities;

    glm::vec3 prevSweepWorldDir{0.f, 0.f, 0.f};

    bool landingShockwaveFired = false;

    bool gripConsumedThisAttack = false;

    bool isActive() const { return def != nullptr; }
};

struct CHealth {
    static constexpr int kSegmentSize = 3;
    static constexpr int kMaxSegments = 3;
    static constexpr float kInvincTime = 1.5f;

    // ─── ダメージ遅延表示 (赤バー) ─────────────────────────
    // 0.5 秒待ってから秒速 kDamageDrainSpeed で減少。
    static constexpr float kDamageDelay = 0.5f;
    static constexpr float kDamageDrainSpeed = 6.f;  // HP/秒、 1 区画を 0.5 秒で減らす

    // ─── potion 拾い時のフラッシュ ──────────────────────────
    static constexpr float kHealFlashDuration = 2.0f;

    int segmentCount = 2;
    int currentHp = kSegmentSize;
    int unlockedSegments = 2;
    float invincTimer = 0.f;

    // 表示用 HP (float、 滑らかに減る)。 -1 = 未初期化、 初回 tick で実 HP に合わせる。
    float displayedTotalHp = -1.f;
    // ダメージ受けた後、 赤バーが減り始めるまでの待機時間。
    float damageDelayTimer = 0.f;
    // potion 拾い時のフラッシュ残り時間 (秒)。
    float healFlashTimer = 0.f;

    int totalHp() const { return (segmentCount - 1) * kSegmentSize + currentHp; }
    bool isDead() const { return segmentCount == 0; }
    bool isInvincible() const { return invincTimer > 0.f; }
    bool canAddSegment() const { return unlockedSegments < kMaxSegments; }

    bool takeDamage(int amount = 1, bool applyInvinc = true) {
        if (isInvincible()) return false;
        currentHp -= amount;
        if (currentHp <= 0) {
            segmentCount--;
            currentHp = (segmentCount > 0) ? kSegmentSize : 0;
        }
        if (applyInvinc) invincTimer = kInvincTime;
        // 赤バー待機タイマー始動 (displayedTotalHp が遅れて追従する)
        damageDelayTimer = kDamageDelay;
        return isDead();
    }

    bool addSegment() {
        if (!canAddSegment()) return false;
        unlockedSegments++;
        segmentCount++;
        return true;
    }

    // 現在区画の currentHp を +amount 回復。 kSegmentSize 上限。
    // 既に満タン (currentHp >= kSegmentSize) なら false (= 回復なし)。
    // 区画をまたいだ回復は行わない (失った区画は復活しない)。
    bool healInCurrentSegment(int amount = 1) {
        if (currentHp >= kSegmentSize) return false;
        currentHp += amount;
        if (currentHp > kSegmentSize) currentHp = kSegmentSize;
        return true;
    }

    void tick(float dt) {
        if (invincTimer > 0.f) {
            invincTimer -= dt;
            if (invincTimer < 0.f) invincTimer = 0.f;
        }
        if (healFlashTimer > 0.f) {
            healFlashTimer -= dt;
            if (healFlashTimer < 0.f) healFlashTimer = 0.f;
        }

        // ─── 表示用 HP の追従 ────
        const float realHp = static_cast<float>(totalHp());

        // 初期化: -1 なら現在値に揃える
        if (displayedTotalHp < 0.f) {
            displayedTotalHp = realHp;
        }

        // 回復で実 HP が表示 HP を超えたら、 即座に追従 (赤バーを残さない)
        if (realHp > displayedTotalHp) {
            displayedTotalHp = realHp;
            damageDelayTimer = 0.f;
        }

        // 表示 HP > 実 HP (= ダメージ受けて遅れて減る途中)
        if (displayedTotalHp > realHp) {
            if (damageDelayTimer > 0.f) {
                damageDelayTimer -= dt;
                if (damageDelayTimer < 0.f) damageDelayTimer = 0.f;
            } else {
                displayedTotalHp -= kDamageDrainSpeed * dt;
                if (displayedTotalHp < realHp) displayedTotalHp = realHp;
            }
        }
    }

    void respawn() {
        segmentCount = unlockedSegments;
        currentHp = kSegmentSize;
        invincTimer = 0.f;
        displayedTotalHp = static_cast<float>(totalHp());
        damageDelayTimer = 0.f;
        healFlashTimer = 0.f;
    }
};

struct PlayerTag {};
struct PlatformTag {};

enum class EnemyState { Chase, Attack, Cooldown };

struct CEnemyAI {
    EnemyState state = EnemyState::Chase;
    float speed = 2.8f;
    float attackRange = 1.6f;
    float attackDuration = 0.30f;
    float attackStartupLag = 0.15f;
    float attackCooldown = 1.20f;
    float timer = 0.f;
    bool punchActive = false;
    float spawnStunTimer = 0.f;
    glm::vec2 moveVelocity = {0.f, 0.f};
    float hitInvincTimer = 0.f;
    bool isDown = false;
    float downTimer = 0.f;

    float hitReactTimer = 0.f;
    bool isDying = false;
    float dyingTimer = 0.f;

    float debugHitFlashTimer = 0.f;
};

enum class ShieldType { None, Iron, Silver, Gold };

struct CShield {
    ShieldType type = ShieldType::None;
    int durability = 0;
    bool guarding = false;

    static int maxDurability(ShieldType t) {
        switch (t) {
            case ShieldType::Iron:
                return 10;
            case ShieldType::Silver:
                return 15;
            case ShieldType::Gold:
                return 20;
            default:
                return 0;
        }
    }

    static const char* typeName(ShieldType t) {
        switch (t) {
            case ShieldType::Iron:
                return "Iron";
            case ShieldType::Silver:
                return "Silver";
            case ShieldType::Gold:
                return "Gold";
            default:
                return "None";
        }
    }

    bool canGuard() const { return type != ShieldType::None && durability > 0; }
};

struct CPickup {
    ShieldType shieldType = ShieldType::Iron;
};

struct ShieldItemTag {};
struct ArmorItemTag {};

struct EnemyTag {};
struct SkeletonTag {};
struct GhostTag {};
struct SoldierTag {};

struct CSkeletalAnim {
    const Model* model = nullptr;
    Animator animator;
    std::vector<glm::mat4> skinMatrices;
    bool playing = true;
    float speed = 1.f;
    SkinBufferPool::Slot skinSlot;
};

struct CAnimState {
    AnimState current = AnimState::Idle;
    AnimState previous = AnimState::Idle;
    float stateTimer = 0.f;
    bool wasOnGround = true;
    float airTime = 0.f;
};

struct CStaticModelRef {
    const Model* sourceModel = nullptr;
};

class Material;
struct CMaterialRef {
    const Material* material = nullptr;
};

struct CEquipment {
    int leftHandBoneIdx = -1;
    const Model* leftHandModel = nullptr;
    glm::mat4 leftHandLocalOffset = glm::mat4(1.f);
    glm::vec3 leftHandScale = {1.f, 1.f, 1.f};
    bool leftHandVisible = true;

    int rightHandBoneIdx = -1;
    const Model* rightHandModel = nullptr;
    glm::mat4 rightHandLocalOffset = glm::mat4(1.f);
    glm::vec3 rightHandScale = {1.f, 1.f, 1.f};
    bool rightHandVisible = true;

    bool hasLeftEquip() const {
        return leftHandVisible && leftHandBoneIdx >= 0 && leftHandModel != nullptr;
    }
    bool hasRightEquip() const {
        return rightHandVisible && rightHandBoneIdx >= 0 && rightHandModel != nullptr;
    }
};

struct CSpin {
    float speedDegPerSec = 90.f;
};

struct CWarpPad {
    StageId targetStage = StageId::Terminal;
    float radius = 1.5f;
};

struct CParticleEmitter {
    int attachBoneIdx = -1;
    glm::mat4 localOffset{1.f};

    particle::EmitterShape shape = particle::EmitterShape::Point;
    glm::vec3 shapeParams{0.f};

    float emitRate = 30.f;
    float accumulator = 0.f;
    bool emitting = true;

    float lifetimeMin = 0.5f;
    float lifetimeMax = 0.8f;
    glm::vec3 emitDirectionLocal{0.f, 1.f, 0.f};
    float speedMin = 1.0f;
    float speedMax = 2.0f;
    float velocityRandomCone = 0.3f;

    glm::vec3 gravity{0.f, 1.5f, 0.f};
    float drag = 0.f;

    glm::vec4 colorStart{1.0f, 1.0f, 0.5f, 1.0f};
    glm::vec4 colorEnd{1.0f, 0.2f, 0.0f, 0.0f};
    float sizeStartMin = 0.08f;
    float sizeStartMax = 0.12f;
    float sizeEndMin = 0.20f;
    float sizeEndMax = 0.30f;

    particle::BlendMode blendMode = particle::BlendMode::Additive;
};

// ─── 動くプラットフォーム ────────────────────────────────
// 5 パターン:
//   PingPongLinear : pos = origin + axisN * amplitude * sin(phase)
//                    任意の軸方向に往復
//   Vertical       : pos.y = origin.y + amplitude * sin(phase)
//                    純粋に上下
//   Circular       : 水平円運動 (XZ 平面)、 半径 amplitude
//   Pendulum       : 振り子。 originPos = 支点 (天井)、 amplitude = 振り子の長さ、
//                    axisN = 振り子が揺れる水平方向 (XZ 平面の任意方向)。
//                    支点から最大 maxAngle (= 60度) まで揺れる。
//                    床は支点の真下、 楕円弧上を移動。
//   OrbitVertical  : 観覧車。 origin = 円の中心、 amplitude = 半径、
//                    axisN = 円の水平成分の向き (axisN, Y の縦面で円運動)。
struct CMovingPlatform {
    enum class Pattern {
        PingPongLinear,
        Vertical,
        Circular,
        Pendulum,
        OrbitVertical,
    };
    Pattern pattern = Pattern::PingPongLinear;

    glm::vec3 originPos{0.f};
    glm::vec3 axis{1.f, 0.f, 0.f};
    float amplitude = 5.f;
    float angularSpeed = 1.f;
    float phase = 0.f;

    glm::vec3 velocity{0.f};
};

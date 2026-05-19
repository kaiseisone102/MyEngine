#define NOMINMAX
// =============================================================================
// enemy_system.cpp — Phase 5-A + デバッグタイマー対応
//
// 段階5-A 変更:
//   - hitReactTimer 減算 + 中は移動・攻撃を停止
//   - isDying 中は AI 処理を完全スキップ (timer も dyingTimer のみ減算)
//   - dyingTimer は別ロジック (simulation_system 側) で死亡判定後に destruct
//
// 追加:
//   - debugHitFlashTimer の減算 (dying 中も継続して減算する。 死亡時に hit
//     フラッシュが赤く見える期間を確保するため)。
// =============================================================================

#include "systems/enemy_system.h"

#include "core/components.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static void updateSkeleton(CTransform& et, CEnemyAI& ai, const glm::vec3& playerPos, float dt,
                           bool isSkeleton) {
    glm::vec3 diff = playerPos - et.pos;
    diff.y = 0.f;
    const float dist = glm::length(diff);
    const float halfZ = isSkeleton ? (et.scale.z * 0.35f) : (et.scale.z * 0.5f);
    const float punchReach = ai.attackRange * 0.5f + halfZ;

    ai.moveVelocity = {0.f, 0.f};

    switch (ai.state) {
        case EnemyState::Chase:
            if (dist > punchReach) {
                if (dist > 0.01f) {
                    const glm::vec3 dir = glm::normalize(diff);
                    ai.moveVelocity = {dir.x * ai.speed * dt, dir.z * ai.speed * dt};
                    et.yaw = glm::degrees(std::atan2(dir.x, dir.z));
                }
            } else {
                ai.state = EnemyState::Attack;
                ai.timer = ai.attackDuration;
                ai.punchActive = false;
                if (dist > 0.01f) et.yaw = glm::degrees(std::atan2(diff.x / dist, diff.z / dist));
            }
            break;

        case EnemyState::Attack:
            ai.timer -= dt;
            {
                const float maxStartup = std::max(0.f, ai.attackDuration - 0.05f);
                const float startup = std::clamp(ai.attackStartupLag, 0.f, maxStartup);
                const float elapsed = ai.attackDuration - ai.timer;
                ai.punchActive = (ai.timer > 0.f) && (elapsed >= startup);
            }
            if (ai.timer <= 0.f) {
                ai.state = EnemyState::Cooldown;
                ai.timer = ai.attackCooldown;
                ai.punchActive = false;
            }
            break;

        case EnemyState::Cooldown:
            ai.timer -= dt;
            if (ai.timer <= 0.f) {
                ai.state = EnemyState::Chase;
                ai.timer = 0.f;
            }
            if (dist > 0.01f) et.yaw = glm::degrees(std::atan2(diff.x / dist, diff.z / dist));
            break;
    }
}

static void updateGhost(CTransform& et, const glm::vec3& playerPos3d, float dt, float speed) {
    glm::vec3 diff = playerPos3d - et.pos;
    const float dist = glm::length(diff);
    if (dist < 0.01f) return;

    const glm::vec3 dir = glm::normalize(diff);
    et.pos += dir * speed * dt;

    if (glm::length(glm::vec2(diff.x, diff.z)) > 0.01f)
        et.yaw = glm::degrees(std::atan2(dir.x, dir.z));
}

// タイマーを 0 まで減算するヘルパ
static inline void tickDown(float& timer, float dt) {
    if (timer > 0.f) {
        timer -= dt;
        if (timer < 0.f) timer = 0.f;
    }
}

void EnemySystem::update(const std::vector<flecs::entity>& enemies, flecs::entity player,
                         float dt) const {
    const glm::vec3 playerPos3d = player.get<CTransform>().pos;

    for (flecs::entity enemy : enemies) {
        CTransform& et = enemy.ensure<CTransform>();
        CEnemyAI& ai = enemy.ensure<CEnemyAI>();

        // debugHitFlashTimer は dying 中も継続して減算する。
        // 「敵が死んだ瞬間に赤くフラッシュ → アニメ中に消える」 を実現するため、
        // dying スキップより前に減算する。
        tickDown(ai.debugHitFlashTimer, dt);

        // Phase 5-A: 死亡アニメ中は AI 処理スキップ。dyingTimer のみ減算。
        if (ai.isDying) {
            ai.moveVelocity = {0.f, 0.f};
            ai.punchActive = false;
            tickDown(ai.dyingTimer, dt);
            continue;
        }

        // ヒット無敵タイマー
        tickDown(ai.hitInvincTimer, dt);
        // Phase 5-A: ヒットリアクションタイマー
        tickDown(ai.hitReactTimer, dt);
        // スポーン硬直タイマー
        tickDown(ai.spawnStunTimer, dt);

        // ダウンタイマー (0 になったタイミングで Chase に復帰)
        if (ai.downTimer > 0.f) {
            ai.downTimer -= dt;
            if (ai.downTimer <= 0.f) {
                ai.downTimer = 0.f;
                ai.isDown = false;
                ai.state = EnemyState::Chase;
            }
        }

        // ダウン中 / spawn 硬直中 / Phase 5-A: ヒットリアクション中 は AI 停止
        if (ai.isDown || ai.spawnStunTimer > 0.f || ai.hitReactTimer > 0.f) {
            ai.moveVelocity = {0.f, 0.f};
            ai.punchActive = false;
            continue;
        }

        if (enemy.has<GhostTag>()) {
            updateGhost(et, playerPos3d, dt, ai.speed);
        } else if (enemy.has<SkeletonTag>() || enemy.has<SoldierTag>()) {
            updateSkeleton(et, ai, playerPos3d, dt, enemy.has<SkeletonTag>());
        }
    }
}

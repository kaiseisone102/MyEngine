#define NOMINMAX
// =============================================================================
// enemy_system.cpp — 敵 AI システム実装
// =============================================================================
// 状態遷移:
//
//   Chase ──(距離 <= attackRange)──► Attack ──(timer <= 0)──► Cooldown
//     ▲                                                           │
//     └──────────────────(timer <= 0)────────────────────────────┘
//
// =============================================================================

#include "systems/enemy_system.h"

#include "core/components.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <algorithm>  // std::clamp
#include <cmath>      // std::atan2
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// =============================================================================
// 内部ヘルパー（骸骨共通の XZ 追尾ステートマシン）
// =============================================================================
static void updateSkeleton(CTransform& et, CEnemyAI& ai, const glm::vec3& playerPos, float dt,
                           bool isSkeleton) {
    // 水平（XZ）距離のみで判定・移動する
    glm::vec3 diff = playerPos - et.pos;
    diff.y = 0.f;
    const float dist = glm::length(diff);
    // 実際のパンチ箱の届く距離に合わせる（開始距離と命中判定のズレをなくす）
    const float halfZ = isSkeleton ? (et.scale.z * 0.35f) : (et.scale.z * 0.5f);
    const float punchReach = ai.attackRange * 0.5f + halfZ;

    // デフォルトで移動量をリセット（停止中 or 攻撃中は動かない）
    ai.moveVelocity = {0.f, 0.f};

    switch (ai.state) {
        case EnemyState::Chase:
            if (dist > punchReach) {
                if (dist > 0.01f) {
                    const glm::vec3 dir = glm::normalize(diff);

                    // pos には直接加算しない。
                    // moveVelocity に記録して、MovementSystem::moveWithSlide に任せる。
                    // moveWithSlide が X/Z を分割適用することで壁沿いスライドが実現する。
                    ai.moveVelocity = {dir.x * ai.speed * dt, dir.z * ai.speed * dt};

                    et.yaw = glm::degrees(std::atan2(dir.x, dir.z));
                }
            } else {
                ai.state = EnemyState::Attack;
                ai.timer = ai.attackDuration;
                ai.punchActive = false;
                // Attack 中は向きを固定するため、開始時点でのみプレイヤー方向を向く
                if (dist > 0.01f) et.yaw = glm::degrees(std::atan2(diff.x / dist, diff.z / dist));
            }
            break;

        case EnemyState::Attack:
            ai.timer -= dt;
            // 攻撃開始ラグ: 予備動作中は判定を出さない
            // elapsed が attackStartupLag を超えたら punchActive を有効化
            {
                // attackStartupLag が attackDuration
                // 以上だと判定窓が消えパンチが出ないため上限を切る
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
            // Attack中は向きを固定（予備動作中も判定発生中も回頭しない）
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

// =============================================================================
// 内部ヘルパー（幽霊の XYZ 3D 追尾）
// =============================================================================
// 幽霊は重力を受けず、空中でもプレイヤーに向かって一直線に飛ぶ。
// プレイヤーへの接触判定はオーケストレーター側で行い、
// 接触したらエンティティを破棄するためここでは移動のみ担当する。
static void updateGhost(CTransform& et, const glm::vec3& playerPos3d, float dt, float speed) {
    glm::vec3 diff = playerPos3d - et.pos;  // 3D ベクトル（Y 含む）
    const float dist = glm::length(diff);
    if (dist < 0.01f) return;

    const glm::vec3 dir = glm::normalize(diff);
    et.pos += dir * speed * dt;

    // 幽霊が向く方向: 水平成分で yaw を決める
    if (glm::length(glm::vec2(diff.x, diff.z)) > 0.01f)
        et.yaw = glm::degrees(std::atan2(dir.x, dir.z));
}

// =============================================================================
// EnemySystem::update
// =============================================================================
void EnemySystem::update(const std::vector<flecs::entity>& enemies, flecs::entity player,
                         float dt) const {
    const glm::vec3 playerPos3d = player.get<CTransform>().pos;

    for (flecs::entity enemy : enemies) {
        CTransform& et = enemy.ensure<CTransform>();
        CEnemyAI& ai = enemy.ensure<CEnemyAI>();

        // ヒット無敵タイマーを毎フレーム減算（プレイヤー攻撃の多段ヒット防止）
        if (ai.hitInvincTimer > 0.f) {
            ai.hitInvincTimer -= dt;
            if (ai.hitInvincTimer < 0.f) ai.hitInvincTimer = 0.f;
        }

        // スポーン硬直タイマー。残っている間は行動停止（敵種ごとの初期値は SpawnSystem で設定）
        if (ai.spawnStunTimer > 0.f) {
            ai.spawnStunTimer -= dt;
            if (ai.spawnStunTimer < 0.f) ai.spawnStunTimer = 0.f;
        }

        // ダウンタイマーを毎フレーム減算し、時間切れで回復
        if (ai.downTimer > 0.f) {
            ai.downTimer -= dt;
            if (ai.downTimer <= 0.f) {
                ai.downTimer = 0.f;
                ai.isDown = false;
                ai.state = EnemyState::Chase;  // 回復後は追跡再開
            }
        }

        // ダウン中 or スポーン硬直中は AI 処理をスキップ（移動も攻撃もしない）
        if (ai.isDown || ai.spawnStunTimer > 0.f) {
            ai.moveVelocity = {0.f, 0.f};
            ai.punchActive = false;
            continue;
        }

        if (enemy.has<GhostTag>()) {
            // 幽霊: 3D 一直線追尾（障害物無視・重力なし）
            updateGhost(et, playerPos3d, dt, ai.speed);
        } else if (enemy.has<SkeletonTag>() || enemy.has<SoldierTag>()) {
            // 骸骨 / ソルジャー: XZ 平面追尾・パンチ・クールダウン
            // ソルジャーは骸骨と同じ AI ロジックを使う（HP のみ異なる）
            updateSkeleton(et, ai, playerPos3d, dt, enemy.has<SkeletonTag>());
        }
        // 未知のタグを持つ敵は何もしない（将来の拡張に備えたサイレントスキップ）
    }
}

// =============================================================================
// physics_system.cpp — Phase 3 (Terrain 接地対応)
// =============================================================================
// 設計:
//   既存の cube AABB sweepAxis(Y) で各 platform との衝突を解決した後、
//   TerrainMesh::sampleHeight で取得した高さで「持ち上げ判定」 を行う。
//
//   entity の pos.y は「足元」 規約 (physics_util.h 参照)。
//   sampleHeight が返す値も「ワールドの地面 y」 なので直接比較できる:
//     if (pos.y < terrainHeight) → 地面に埋まっている → 押し上げる
//
//   範囲外 (terrain->sampleHeight が lowest を返す) は接地判定しない
//   (= 範囲外に出たら落下、 リスポーン処理に任せる)。
// =============================================================================
#include "systems/physics_system.h"

#include <algorithm>
#include <limits>

#include "renderer/terrain_mesh.h"
#include "systems/physics_util.h"

namespace {

// Terrain 接地判定の共通処理。
// pos.y が terrain 高さより低ければ持ち上げ、 vy を 0 にクランプ、 onGround を true に。
// 戻り値: terrain によって接地したか
bool resolveTerrainGround(CTransform& t, CVelocity& v, const TerrainMesh* terrain) {
    if (!terrain) return false;
    const float th = terrain->sampleHeight(t.pos.x, t.pos.z);
    if (th == std::numeric_limits<float>::lowest()) return false;  // 範囲外
    if (t.pos.y < th) {
        t.pos.y = th;
        if (v.y < 0.f) v.y = 0.f;
        return true;
    }
    // 完全に上空 (terrain よりも上) なら接地ではない
    return false;
}

// findGroundY: entity の現在 XZ 位置で「届く最高の地面 y」 を返す。
// platforms の top と terrain の高さの両方を見て、 最大値を取る。
// player の XZ が platform の XZ 範囲内にある場合のみ「乗れる」 と判定。
// 何も見つからなければ std::numeric_limits<float>::lowest() を返す。
float findGroundY(const CTransform& t, const std::vector<flecs::entity>& platforms,
                  const TerrainMesh* terrain) {
    float bestY = std::numeric_limits<float>::lowest();

    // 1. Terrain (範囲内なら採用)
    if (terrain) {
        const float th = terrain->sampleHeight(t.pos.x, t.pos.z);
        if (th != std::numeric_limits<float>::lowest()) {
            bestY = std::max(bestY, th);
        }
    }

    // 2. Platforms (entity の XZ が platform の XZ 範囲内なら、 platform の top を候補に)
    const float ex = t.pos.x;
    const float ez = t.pos.z;
    const float ehalfX = t.scale.x * 0.5f;
    const float ehalfZ = t.scale.z * 0.5f;
    for (flecs::entity plat : platforms) {
        if (!plat.is_alive()) continue;
        const auto& pt = plat.get<CTransform>();
        const float phalfX = pt.scale.x * 0.5f;
        const float phalfZ = pt.scale.z * 0.5f;
        // XZ overlap 判定
        if (ex + ehalfX < pt.pos.x - phalfX) continue;
        if (ex - ehalfX > pt.pos.x + phalfX) continue;
        if (ez + ehalfZ < pt.pos.z - phalfZ) continue;
        if (ez - ehalfZ > pt.pos.z + phalfZ) continue;
        const float top = pt.pos.y + pt.scale.y;  // 足元基準なので top = pos.y + scale.y
        bestY = std::max(bestY, top);
    }

    return bestY;
}

// trySnapToGround: 「前フレーム接地中で、 重力で離地しただけ」 の場合に地面に snap する。
// 下り坂や階段でのフリッカリング (接地/離地の高速繰り返し) を防ぐ。
// 戻り値: snap 実行した場合 true
bool trySnapToGround(CTransform& t, CVelocity& v, bool wasOnGround, bool jumpingUp, bool jumpReq,
                     const std::vector<flecs::entity>& platforms, const TerrainMesh* terrain) {
    constexpr float kSnapDistance = 0.3f;  // 30cm 以内なら snap

    // 条件: 前フレーム接地、 上向き速度なし、 ジャンプ要求なし
    if (!wasOnGround || jumpingUp || jumpReq) return false;

    const float groundY = findGroundY(t, platforms, terrain);
    if (groundY == std::numeric_limits<float>::lowest()) return false;

    const float heightAbove = t.pos.y - groundY;
    if (heightAbove < 0.f || heightAbove >= kSnapDistance) return false;

    // Snap!
    t.pos.y = groundY;
    if (v.y < 0.f) v.y = 0.f;
    return true;
}

}  // namespace

AABB PhysicsSystem::entityAABB(flecs::entity e) { return physics::entityAABB(e); }

void PhysicsSystem::resolveVerticalCollisions(flecs::entity player,
                                              const std::vector<flecs::entity>& platforms,
                                              const TerrainMesh* terrain) const {
    auto& pt = player.ensure<CTransform>();
    auto& pv = player.ensure<CVelocity>();
    auto& pp = player.ensure<CPhysics>();
    pp.onGround = false;

    // ─── 1. cube platform との AABB Y軸衝突解決 ───────────────
    for (flecs::entity plat : platforms) {
        const AABB platBox = physics::entityAABB(plat);
        const AABB playerBox = physics::entityAABB(player);

        const AABBHit hit = playerBox.sweepAxis(platBox, AABBHit::Axis::Y);
        if (!hit.overlap) continue;

        pt.pos.y += hit.normal.y * hit.depth;

        if (hit.isFromTop()) {
            if (pv.y < 0.f) pv.y = 0.f;
            pp.jumpsRemaining = pp.maxJumps;
            pp.usedDoubleJump = false;
            pp.onGround = true;
        } else if (hit.isFromBottom()) {
            if (pv.y > 0.f) pv.y = 0.f;
        }
    }

    // ─── 2. TerrainMesh との接地判定 ──────────────────────────
    // cube platform で押し上げられた後でも、 terrain がもっと高い場所なら
    // さらに押し上げる (= 一番高い地面に着地)。
    if (resolveTerrainGround(pt, pv, terrain)) {
        pp.jumpsRemaining = pp.maxJumps;
        pp.usedDoubleJump = false;
        pp.onGround = true;
    }
}

bool PhysicsSystem::resolveVerticalForEntity(flecs::entity entity,
                                             const std::vector<flecs::entity>& platforms,
                                             const TerrainMesh* terrain) const {
    auto& et = entity.ensure<CTransform>();
    auto& ev = entity.ensure<CVelocity>();
    bool onGround = false;

    for (flecs::entity plat : platforms) {
        const AABB platBox = physics::entityAABB(plat);
        const AABB ebox = physics::entityAABB(entity);

        const AABBHit hit = ebox.sweepAxis(platBox, AABBHit::Axis::Y);
        if (!hit.overlap) continue;

        et.pos.y += hit.normal.y * hit.depth;

        if (hit.isFromTop()) {
            if (ev.y < 0.f) ev.y = 0.f;
            onGround = true;
        } else if (hit.isFromBottom()) {
            if (ev.y > 0.f) ev.y = 0.f;
        }
    }

    // Terrain 接地判定
    if (resolveTerrainGround(et, ev, terrain)) {
        onGround = true;
    }
    return onGround;
}

bool PhysicsSystem::applyEnemyGravity(flecs::entity enemy,
                                      const std::vector<flecs::entity>& platforms,
                                      const TerrainMesh* terrain, float dt, float gravity) const {
    auto& et = enemy.ensure<CTransform>();
    auto& ev = enemy.ensure<CVelocity>();

    const float g = (ev.y < 0.f) ? gravity * 1.5f : gravity;
    ev.y += g * dt;
    if (ev.y < -40.f) ev.y = -40.f;

    // 重力適用前に前フレームの接地状態を保存。 敵は CPhysics を持たない場合があるので、
    // 「前フレームで地面の上に居たか」 は ev.y がほぼ 0 だったかで近似判定する。
    // (= 0 にクランプされた状態が「接地中」 の証拠)
    const bool wasOnGround = (ev.y - g * dt) >= -0.05f && (ev.y - g * dt) <= 0.05f;

    et.pos.y += ev.y * dt;
    bool onGround = resolveVerticalForEntity(enemy, platforms, terrain);

    // ─── 接地スナップ ──────────────────────────────────────────
    // 敵は jump しないので jumpingUp は常に false、 jumpReq も false。
    if (wasOnGround && !onGround) {
        if (trySnapToGround(et, ev, wasOnGround, /*jumpingUp*/ false, /*jumpReq*/ false, platforms,
                            terrain)) {
            onGround = true;
        }
    }

    if (et.pos.y < -10.f) {
        et.pos.y = 3.f;
        ev.y = 0.f;
    }
    return onGround;
}

bool PhysicsSystem::update(flecs::entity player, const std::vector<flecs::entity>& platforms,
                           const TerrainMesh* terrain, float dt, float gravity,
                           float jumpSpeed) const {
    auto& pt = player.ensure<CTransform>();
    auto& pv = player.ensure<CVelocity>();
    auto& pp = player.ensure<CPhysics>();

    CAttack& atk = player.ensure<CAttack>();
    const bool diveLiftPhase = atk.isActive() && atk.isDiving && !atk.diveDropStarted;
    if (diveLiftPhase) {
        const float targetY = atk.diveLiftStartY + atk.diveLiftHeight;
        constexpr float kLiftSpeed = 14.f;
        constexpr float kDiveSpeed = -18.f;

        if (pt.pos.y < targetY - 1e-4f) {
            const float remaining = targetY - pt.pos.y;
            const float step = std::min(remaining, kLiftSpeed * dt);
            const float yBefore = pt.pos.y;
            pt.pos.y += step;
            pv.y = 0.f;
            resolveVerticalCollisions(player, platforms, terrain);
            if (pt.pos.y <= yBefore + 1e-5f && step > 1e-5f) {
                atk.diveDropStarted = true;
                pv.y = kDiveSpeed;
            } else if (pt.pos.y >= targetY - 1e-4f) {
                pt.pos.y = targetY;
                atk.diveDropStarted = true;
                pv.y = kDiveSpeed;
            } else {
                pp.jumpReq = false;
                if (pt.pos.y < -10.f) {
                    pt.pos = {0.f, 3.f, 0.f};
                    pv.y = 0.f;
                    return true;
                }
                return false;
            }
        } else {
            atk.diveDropStarted = true;
            pv.y = kDiveSpeed;
        }
        if (atk.diveDropStarted) pp.jumpReq = false;
    }

    const float g = (pv.y < 0.f) ? gravity * 1.5f : gravity;
    pv.y += g * dt;
    if (pv.y < -40.f) pv.y = -40.f;

    // 重力適用前の接地状態を保存 (snap 判定に使う)
    const bool wasOnGround = pp.onGround;
    const bool jumpingUp = pv.y > 0.1f;

    pt.pos.y += pv.y * dt;
    resolveVerticalCollisions(player, platforms, terrain);

    // ─── 接地スナップ ──────────────────────────────────────────
    // 前フレーム接地中で、 上向き速度なし、 ジャンプ要求なしで、
    // 重力で離地 (pp.onGround = false) しただけなら 50cm 以内で snap。
    // 下り坂や階段でのフリッカリング (Walk↔Jump 高速繰り返し) を防ぐ。
    if (wasOnGround && !pp.onGround && !jumpingUp && !pp.jumpReq) {
        if (trySnapToGround(pt, pv, wasOnGround, jumpingUp, pp.jumpReq, platforms, terrain)) {
            pp.onGround = true;
            pp.jumpsRemaining = pp.maxJumps;
            pp.usedDoubleJump = false;
        }
    }

    if (pp.jumpReq && pp.jumpsRemaining > 0) {
        if (pp.jumpsRemaining == 1) {
            pp.usedDoubleJump = true;
        }
        pv.y = jumpSpeed;
        pp.onGround = false;
        pp.jumpsRemaining--;
    }
    pp.jumpReq = false;

    if (pt.pos.y < -10.f) {
        pt.pos = {0.f, 3.f, 0.f};
        pv.y = 0.f;
        return true;
    }
    return false;
}

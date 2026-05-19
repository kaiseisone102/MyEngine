#pragma once
// =============================================================================
// core/grave_fx.h — 墓専用パーティクル制御 (青い蛍)
// =============================================================================
// spirit が出る墓 (CGrave 持ち) に attach する青い蛍エフェクト。
// 状態:
//   Intact   → emitter ON (青蛍)
//   Damaged  → emitter ON (青蛍)
//   Destroyed → emitter OFF (光が消える)
//
// 既存粒子は emit 停止後も寿命まで残る (= ふわっと消える)。
//
// grip と同じパターン:
//   attachEmitter(grave): 墓 entity に CParticleEmitter を attach
//   syncEmitter(grave):   CGrave.state に応じて emitting を切替
// =============================================================================

#include <flecs.h>

namespace grave_fx {

// 墓 entity に蛍 emitter を attach。 spawn 時に 1 回呼ぶ。
void attachEmitter(flecs::entity grave);

// 墓の CGrave.state に応じて emitter の emitting を ON/OFF。
// 状態遷移時 (Intact → Damaged、 → Destroyed) に呼ぶ。
void syncEmitter(flecs::entity grave);

}  // namespace grave_fx

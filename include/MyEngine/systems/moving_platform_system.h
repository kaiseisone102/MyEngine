#pragma once
// =============================================================================
// systems/moving_platform_system.h — 動くプラットフォーム
// =============================================================================
// 役割:
//   - CMovingPlatform を持つ全 entity の CTransform::pos を更新
//   - 各 platform の現フレーム速度を CMovingPlatform::velocity に書く
//   - 「player が乗ってる platform」 の速度に従って player の pos を運ぶ
//     (前フレームの physics で確定した CPhysics::standingOn を参照)
//
// 呼び出し場所:
//   simulation_system.cpp の updatePlayer の冒頭 (input 処理より前)。
//   これで「player が input する前に platform に運ばれる」 自然な順序になる。
//
// 慣性 (空中時):
//   乗ってる間は CVelocity::xz を platform 速度の XZ 成分と同期する。
//   ジャンプや踏み外しで離脱すると CVelocity::xz はそのまま残る → 慣性として効く。
// =============================================================================

#include <flecs.h>

struct WorldData;

class MovingPlatformSystem {
   public:
    void update(WorldData& wd, float dt) const;
};

// include/MyEngine/systems/skeletal_anim_system.h
#pragma once
// =============================================================================
// skeletal_anim_system.h — Phase 3-A
// =============================================================================
// 役割:
//   1. CAnimState.current が前フレームから変化していたら、対応するアニメ
//      クリップを Animator.setClip で切り替える
//   2. 全 CSkeletalAnim エンティティの Animator を進行させ skinMatrix 計算
// =============================================================================

class GameState;

class SkeletalAnimSystem {
   public:
    void update(GameState& gameState, float dt) const;
};

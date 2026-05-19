// =============================================================================
// audio_event_system.cpp
// =============================================================================
// Plays SE based on player's grounded state and jump events.
//
//   - syncGroundState : record initial grounded state (avoid false detection)
//   - onJumpRequested : if grounded and not input-locked, play jump SE
//   - onPostPhysics   : detect transition (air -> ground) and play land SE
//
// Note:
//   - CPhysics is required on player. Entities without CPhysics or invalid
//     are skipped (no-op).
// =============================================================================
#include "systems/audio_event_system.h"

void AudioEventSystem::syncGroundState(flecs::entity player) {
    if (!player.is_valid()) return;
    if (!player.has<CPhysics>()) return;
    const CPhysics& phys = player.get<CPhysics>();
    wasOnGround_ = phys.onGround;
}

void AudioEventSystem::onJumpRequested(flecs::entity player, bool inputLocked,
                                        SoundManager& sound) const {
    if (inputLocked) return;
    if (!player.is_valid()) return;
    if (!player.has<CPhysics>()) return;
    const CPhysics& phys = player.get<CPhysics>();
    if (phys.onGround) {
        sound.playJump();
    }
}

void AudioEventSystem::onPostPhysics(flecs::entity player, SoundManager& sound) {
    if (!player.is_valid()) return;
    if (!player.has<CPhysics>()) return;
    const CPhysics& phys = player.get<CPhysics>();
    const bool nowOnGround = phys.onGround;
    if (!wasOnGround_ && nowOnGround) {
        sound.playLand();
    }
    wasOnGround_ = nowOnGround;
}

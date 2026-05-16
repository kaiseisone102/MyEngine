#include "systems/audio_event_system.h"

void AudioEventSystem::syncGroundState(flecs::entity player) {
    wasOnGround_ = player.get<CPhysics>().onGround;
}

void AudioEventSystem::onJumpRequested(flecs::entity player, bool inputLocked,
                                       SoundManager& sound) const {
    if (inputLocked) return;
    if (player.get<CPhysics>().onGround) sound.playJump();
}

void AudioEventSystem::onPostPhysics(flecs::entity player, SoundManager& sound) {
    const bool nowGround = player.get<CPhysics>().onGround;
    const float velY = player.get<CVelocity>().y;
    if (!wasOnGround_ && nowGround && velY >= 0.f) {
        sound.playLand();
    }
    wasOnGround_ = nowGround;
}

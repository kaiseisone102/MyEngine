#pragma once
// =============================================================================
// systems/spirit_system.h
// =============================================================================
#include <flecs.h>
#include <glm/glm.hpp>

struct WorldData;

class SpiritSystem {
   public:
    void update(WorldData& wd, float dt) const;
    static flecs::entity spawnSpirit(WorldData& wd, const glm::vec3& origin);
};

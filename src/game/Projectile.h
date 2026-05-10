#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace game {

struct Projectile {
    glm::vec3 position { 0.0f };
    glm::vec3 velocity { 0.0f };
    float     age      = 0.0f;
    float     maxAge   = 3.0f;
    float     scale    = 0.05f;
    int       explosiveDamage = 0;  // >0 = burst on impact, dealing this dmg in a small AoE
    int       lightningDamage = 0;  // >0 = on hit, chain this dmg to N nearest enemies
    bool      sticky   = false;     // when true, the projectile freezes on terrain contact

    // Stuck-in-enemy state. When stuckEnemyId != 0 the projectile's
    // position is driven each frame by the host enemy (lookup by id) +
    // stuckOffset. Velocity is preserved as the impact direction so the
    // model still orients along the way it came in.
    uint32_t  stuckEnemyId = 0;
    glm::vec3 stuckOffset  { 0.0f };

    bool alive() const { return age < maxAge; }
    bool stuck() const { return stuckEnemyId != 0; }
};

}  // namespace game

#pragma once

#include <glm/glm.hpp>

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

    bool alive() const { return age < maxAge; }
};

}  // namespace game

#pragma once

#include <glm/glm.hpp>

namespace game {

struct Projectile {
    glm::vec3 position { 0.0f };
    glm::vec3 velocity { 0.0f };
    float     age      = 0.0f;
    float     maxAge   = 3.0f;
    float     scale    = 0.05f;

    bool alive() const { return age < maxAge; }
};

}  // namespace game

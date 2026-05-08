#pragma once

#include "Enemy.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace game {

// Fixed-interval ring spawner. Drops enemies on a circle of radius
// spawnRadius around the player. Will eventually take the level/nav-mesh
// to bias spawn locations and avoid placing into walls.
class EnemySpawner {
public:
    float intervalSec = 4.0f;
    float spawnRadius = 15.0f;

    void update(float dt, std::vector<Enemy>& enemies,
                const glm::vec3& playerPos);

    // For external "spawn now" hooks (debug, scripted events).
    void spawnAt(std::vector<Enemy>& enemies, const glm::vec3& at);

private:
    float    timer_  = 1.5f;          // first wave shortly after start
    uint32_t rngState_ = 0xDEADBEEFu;

    float rand01_();
};

}  // namespace game

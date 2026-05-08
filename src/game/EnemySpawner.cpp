#include "EnemySpawner.h"

#include <cmath>

namespace game {

float EnemySpawner::rand01_() {
    rngState_ = rngState_ * 1664525u + 1013904223u;  // numerical-recipes LCG
    return static_cast<float>(rngState_ >> 8) / static_cast<float>(1u << 24);
}

void EnemySpawner::spawnAt(std::vector<Enemy>& enemies, const glm::vec3& at) {
    Enemy e;
    e.position = glm::vec3(at.x, 0.0f, at.z);
    enemies.push_back(e);
}

void EnemySpawner::update(float dt, std::vector<Enemy>& enemies,
                          const glm::vec3& playerPos) {
    timer_ -= dt;
    while (timer_ <= 0.0f) {
        timer_ += intervalSec;
        const float angle = rand01_() * 6.28318530718f;
        const glm::vec3 at = playerPos + glm::vec3(
            std::cos(angle) * spawnRadius, 0.0f,
            std::sin(angle) * spawnRadius);
        spawnAt(enemies, at);
    }
}

}  // namespace game

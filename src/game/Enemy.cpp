#include "Enemy.h"

#include <glm/geometric.hpp>

namespace game {

void Enemy::update(float dt, const glm::vec3& playerPos) {
    glm::vec3 dir = playerPos - position;
    dir.y = 0.0f;
    const float d = glm::length(dir);
    if (d > 1e-3f) dir /= d;
    velocity = dir * kEnemyMoveSpeed;
    position += velocity * dt;
    position.y = 0.0f;  // pinned to floor until level collision exists
}

}  // namespace game

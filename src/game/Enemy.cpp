#include "Enemy.h"

#include <glm/geometric.hpp>

namespace game {

void Enemy::update(float dt, const glm::vec3& playerPos) {
    // Pathfind in the XZ plane only. Y is set once at spawn (to terrain
    // height) and never moved again — keeps small models from bobbing as
    // they cross varied terrain on their way to the player.
    glm::vec3 dir = playerPos - position;
    dir.y = 0.0f;
    const float d = glm::length(dir);
    if (d > 1e-3f) dir /= d;
    const float speed = (def ? def->moveSpeed : kEnemyMoveSpeed) * speedMult;
    velocity = dir * speed;
    velocity.y = 0.0f;
    position.x += velocity.x * dt;
    position.z += velocity.z * dt;
    if (attackCooldown > 0.0f) attackCooldown -= dt;
}

}  // namespace game

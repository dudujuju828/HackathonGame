#pragma once

#include "EnemyDef.h"
#include "render/Animator.h"
#include <glm/glm.hpp>

namespace game {

// Fallback constants used when no EnemyDef is assigned (should not occur at runtime).
constexpr float kEnemyScale      = 0.30f;
constexpr float kEnemyHeight     = 1.70f;
constexpr float kEnemyRadius     = 0.60f;
constexpr float kEnemyMoveSpeed  = 1.3f;
constexpr int   kEnemyMaxHp      = 2;
constexpr int   kEnemyXpReward   = 30;

struct Enemy {
    glm::vec3 position { 0.0f };
    glm::vec3 velocity { 0.0f };
    int       hp       = kEnemyMaxHp;
    float     attackCooldown = 0.0f;  // seconds until this enemy can hit again

    const EnemyDef*  def = nullptr;  // assigned at spawn; never null after that
    render::Animator animator;

    bool alive() const { return hp > 0; }

    void update(float dt, const glm::vec3& playerPos);

    glm::vec3 hitCentre() const {
        float h = def ? def->height : kEnemyHeight;
        return position + glm::vec3(0.0f, h, 0.0f);
    }
};

} // namespace game

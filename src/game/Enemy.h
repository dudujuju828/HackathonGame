#pragma once

#include "render/Animator.h"
#include <glm/glm.hpp>

namespace game {

// Per-enemy tunables. Tweak in one place to scale difficulty / vibe.
constexpr float kEnemyRadius     = 0.55f;   // collision sphere (chest height)
constexpr float kEnemyHeight     = 1.7f;    // for placement / centre offset
constexpr float kEnemyMoveSpeed  = 1.8f;    // m/s; deliberate, dread-pace shamble
constexpr int   kEnemyMaxHp      = 2;       // dies in 2 syringe hits
constexpr int   kEnemyXpReward   = 30;      // xp granted on death

struct Enemy {
    glm::vec3 position { 0.0f };
    glm::vec3 velocity { 0.0f };
    int       hp       = kEnemyMaxHp;

    render::Animator animator;

    bool alive() const { return hp > 0; }

    // Direct chase on the XZ plane. Real pathfinding lands when walls do.
    void update(float dt, const glm::vec3& playerPos);

    // Centre of the collision sphere in world space (chest height).
    glm::vec3 hitCentre() const {
        return position + glm::vec3(0.0f, kEnemyHeight * 0.5f, 0.0f);
    }
};

}  // namespace game

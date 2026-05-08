#pragma once

#include "render/Animator.h"
#include <glm/glm.hpp>

namespace game {

// Per-enemy tunables. Tweak in one place to scale difficulty / vibe.
//
// The render offset and the hit-sphere centre both read from kEnemyHeight so
// they can't drift apart when the model placement is retuned. The radius is
// sized to roughly match the silhouette of the Harpy GLB scaled by kEnemyScale.
constexpr float kEnemyScale      = 0.3f;    // uniform scale applied to the GLB at draw time
constexpr float kEnemyHeight     = 1.7f;    // world-space Y offset for model origin + hit centre
constexpr float kEnemyRadius     = 0.6f;    // collision sphere radius (world units; intentionally generous so syringes feel snappy)
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

    // Centre of the collision sphere — anchored to the same offset the
    // renderer uses, so projectile hits line up with the visible model.
    glm::vec3 hitCentre() const {
        return position + glm::vec3(0.0f, kEnemyHeight, 0.0f);
    }
};

}  // namespace game

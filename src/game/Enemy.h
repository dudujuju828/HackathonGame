#pragma once

#include "EnemyDef.h"
#include "render/Animator.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace game {

// Fallback constants used when no EnemyDef is assigned (should not occur at runtime).
constexpr float kEnemyScale      = 0.30f;
constexpr float kEnemyHeight     = 1.70f;
constexpr float kEnemyRadius     = 0.60f;
constexpr float kEnemyMoveSpeed  = 1.3f;
constexpr int   kEnemyMaxHp      = 2;
constexpr int   kEnemyXpReward   = 30;

struct Enemy {
    // Stable identity for the lifetime of this enemy instance — used by
    // stuck projectiles (and anything else that wants to track a specific
    // enemy across vector reallocations). 0 means "unassigned".
    uint32_t  id       = 0;
    glm::vec3 position { 0.0f };
    glm::vec3 velocity { 0.0f };
    int       hp       = kEnemyMaxHp;
    float     attackCooldown = 0.0f;  // seconds until this enemy can hit again
    float     speedMult      = 1.0f;  // runtime multiplier on def->moveSpeed (aggro etc.)
    float     stepTimer      = 0.0f;  // seconds until next footstep one-shot fires

    // Death animation state. When dying, the enemy's hp is 0 (so alive() is
    // false) but the instance lingers until deathTimer >= deathDuration so
    // the death clip can finish + the body can fade out.
    bool      dying          = false;
    float     deathTimer     = 0.0f;
    float     deathDuration  = 0.0f;

    const EnemyDef*              def         = nullptr;  // assigned at spawn; never null after that
    const render::AnimationClip* currentAnim = nullptr;  // tracks active clip so we don't reset on every frame
    render::Animator             animator;

    bool alive() const { return hp > 0; }

    void update(float dt, const glm::vec3& playerPos);

    glm::vec3 hitCentre() const {
        float h = def ? def->height : kEnemyHeight;
        return position + glm::vec3(0.0f, h, 0.0f);
    }
};

} // namespace game

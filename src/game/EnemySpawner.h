#pragma once

#include "Enemy.h"
#include "EnemyDef.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace game {

// Fixed-interval ring spawner. Drops enemies on a circle of radius
// spawnRadius around the player. Picks a random type from the registered
// EnemyDef table each spawn so all variants appear in the wave.
class EnemySpawner {
public:
    float intervalSec = 4.0f;
    float spawnRadius = 15.0f;

    // Must be called before the first update. defs must outlive the spawner.
    void setDefs(const EnemyDef* defs, int count);

    void update(float dt, std::vector<Enemy>& enemies,
                const glm::vec3& playerPos);

    void spawnAt(std::vector<Enemy>& enemies, const glm::vec3& at);

private:
    float    timer_     = 1.5f;
    uint32_t rngState_  = 0xDEADBEEFu;

    const EnemyDef* defs_     = nullptr;
    int             defCount_ = 0;

    float rand01_();
};

} // namespace game

#pragma once

#include "Enemy.h"
#include "EnemyDef.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace game {

// Spawns enemies on a ring around the player. The legacy `update()` path
// picks random types on a fixed interval; the wave system bypasses it and
// drives spawns directly via `spawnAtIndex`.
class EnemySpawner {
public:
    float intervalSec    = 4.0f;
    float spawnMinRadius = 12.0f;  // metres — closest an enemy may spawn from the player
    float spawnRadius    = 18.0f;  // metres — farthest spawn distance (max of the random range)

    // Must be called before the first update. defs must outlive the spawner.
    void setDefs(const EnemyDef* defs, int count);

    int  defCount() const { return defCount_; }
    const EnemyDef* defs() const { return defs_; }

    // Returns the index in the def table for `name` (case-insensitive),
    // or -1 if no def matches.
    int  findDefIndex(const std::string& name) const;

    // Multiplier applied to each enemy's spawn HP. Set by main from a
    // scaling function so newly-spawned enemies match player power.
    void  setHpMultiplier(float m);
    float hpMultiplier() const { return hpMultiplier_; }

    // Legacy random-pick spawner used when no WaveManager is driving things.
    void update(float dt, std::vector<Enemy>& enemies,
                const glm::vec3& playerPos);

    // Manual spawn paths. spawnAt picks a random def; spawnAtIndex uses
    // a specific def. Both place the enemy at `at`.
    void spawnAt(std::vector<Enemy>& enemies, const glm::vec3& at);
    void spawnAtIndex(std::vector<Enemy>& enemies, const glm::vec3& at, int defIndex);

    // Random ring position around `centre` at `spawnRadius`.
    glm::vec3 ringPositionAround(const glm::vec3& centre);

private:
    float    timer_     = 1.5f;
    uint32_t rngState_  = 0xDEADBEEFu;

    const EnemyDef* defs_         = nullptr;
    int             defCount_     = 0;
    float           hpMultiplier_ = 1.0f;

    float rand01_();
};

} // namespace game

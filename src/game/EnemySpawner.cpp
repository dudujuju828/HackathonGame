#include "EnemySpawner.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace game {

namespace {
bool ieq(const char* a, const char* b) {
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a)) !=
            std::tolower(static_cast<unsigned char>(*b))) return false;
        ++a; ++b;
    }
    return *a == 0 && *b == 0;
}
}  // namespace

float EnemySpawner::rand01_() {
    rngState_ = rngState_ * 1664525u + 1013904223u;  // numerical-recipes LCG
    return static_cast<float>(rngState_ >> 8) / static_cast<float>(1u << 24);
}

void EnemySpawner::setDefs(const EnemyDef* defs, int count) {
    defs_     = defs;
    defCount_ = count;
}

int EnemySpawner::findDefIndex(const std::string& name) const {
    for (int i = 0; i < defCount_; ++i) {
        if (defs_[i].name && ieq(defs_[i].name, name.c_str())) return i;
    }
    return -1;
}

glm::vec3 EnemySpawner::ringPositionAround(const glm::vec3& centre) {
    const float angle = rand01_() * 6.28318530718f;
    const float lo    = std::min(spawnMinRadius, spawnRadius);
    const float hi    = std::max(spawnMinRadius, spawnRadius);
    const float r     = lo + rand01_() * (hi - lo);
    return centre + glm::vec3(std::cos(angle) * r, 0.0f,
                              std::sin(angle) * r);
}

void EnemySpawner::spawnAt(std::vector<Enemy>& enemies, const glm::vec3& at) {
    if (defCount_ <= 0) return;
    int idx = static_cast<int>(rand01_() * static_cast<float>(defCount_)) % defCount_;
    spawnAtIndex(enemies, at, idx);
}

void EnemySpawner::spawnAtIndex(std::vector<Enemy>& enemies, const glm::vec3& at, int defIndex) {
    if (!defs_ || defIndex < 0 || defIndex >= defCount_) return;
    Enemy e;
    e.position = glm::vec3(at.x, 0.0f, at.z);
    e.def = &defs_[defIndex];
    // Scale base HP by the spawner's multiplier so wave totals track player power.
    const float scaledHp = static_cast<float>(e.def->maxHp) * hpMultiplier_;
    e.hp = std::max(1, static_cast<int>(std::round(scaledHp)));
    e.animator.setAnimation(e.def->walkAnim);
    enemies.push_back(e);
}

void EnemySpawner::setHpMultiplier(float m) {
    hpMultiplier_ = std::max(1.0f, m);
}

void EnemySpawner::update(float dt, std::vector<Enemy>& enemies,
                          const glm::vec3& playerPos) {
    timer_ -= dt;
    while (timer_ <= 0.0f) {
        timer_ += intervalSec;
        spawnAt(enemies, ringPositionAround(playerPos));
    }
}

}  // namespace game

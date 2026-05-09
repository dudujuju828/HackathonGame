#pragma once

#include <random>

namespace game {

// Single shared mt19937 for gameplay rolls (chest drops, loot rarity, etc.).
// Seeded once from std::random_device on first use. mt19937 is a
// well-tested PRNG with a long period — preferred over the per-system LCGs
// used elsewhere when fairness matters.
inline std::mt19937& rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

// Uniform float in [0, 1).
inline float rand01() {
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng());
}

}  // namespace game

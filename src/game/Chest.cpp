#include "Chest.h"

#include "Rng.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace game {

Rarity rollChestRarity() {
    // 50 / 25 / 15 / 8 / 2 — cumulative thresholds.
    const float r = rand01();
    if (r < 0.50f) return Rarity::Common;
    if (r < 0.75f) return Rarity::Uncommon;
    if (r < 0.90f) return Rarity::Rare;
    if (r < 0.98f) return Rarity::Epic;
    return Rarity::Legendary;
}

ItemId rollChestItem(Rarity rarity) {
    auto poolFor = [](Rarity r, std::vector<ItemId>& out) {
        out.clear();
        for (int i = 0; i < kItemIdCount; ++i) {
            const auto id = static_cast<ItemId>(i);
            if (itemRarity(id) == r) out.push_back(id);
        }
    };

    std::vector<ItemId> pool;
    poolFor(rarity, pool);
    // Walk down the rarity tiers if nothing is registered at this one.
    while (pool.empty() && static_cast<int>(rarity) > 0) {
        rarity = static_cast<Rarity>(static_cast<int>(rarity) - 1);
        poolFor(rarity, pool);
    }
    if (pool.empty()) return ItemId::AutoSyringeRing;

    int idx = static_cast<int>(rand01() * static_cast<float>(pool.size()));
    idx = std::clamp(idx, 0, static_cast<int>(pool.size()) - 1);
    return pool[idx];
}

glm::vec3 Chest::currentTint() const {
    switch (state) {
        case ChestState::Closed:  return { 1.0f, 1.0f, 1.0f };
        case ChestState::Opening: return cycleTint;
        case ChestState::Fading:
        case ChestState::Done:    return rarityColor(rolled);
    }
    return { 1.0f, 1.0f, 1.0f };
}

float Chest::currentAlpha() const {
    if (state == ChestState::Fading) {
        return std::clamp(1.0f - fadeTimer / kChestFadeDuration, 0.0f, 1.0f);
    }
    return 1.0f;
}

}  // namespace game

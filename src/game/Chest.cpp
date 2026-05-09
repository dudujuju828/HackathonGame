#include "Chest.h"

#include "Rng.h"

#include <algorithm>
#include <cmath>

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

ItemId rollChestItem() {
    const int idx = static_cast<int>(rand01() * static_cast<float>(kItemIdCount));
    return static_cast<ItemId>(std::clamp(idx, 0, kItemIdCount - 1));
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

#pragma once

#include <glm/glm.hpp>

namespace game {

enum class Rarity {
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary,
};

constexpr int kRarityCount = 5;

inline const char* rarityName(Rarity r) {
    switch (r) {
        case Rarity::Common:    return "COMMON";
        case Rarity::Uncommon:  return "UNCOMMON";
        case Rarity::Rare:      return "RARE";
        case Rarity::Epic:      return "EPIC";
        case Rarity::Legendary: return "LEGENDARY";
    }
    return "?";
}

inline glm::vec3 rarityColor(Rarity r) {
    switch (r) {
        case Rarity::Common:    return { 0.75f, 0.75f, 0.75f };
        case Rarity::Uncommon:  return { 0.30f, 0.95f, 0.40f };
        case Rarity::Rare:      return { 0.30f, 0.55f, 1.00f };
        case Rarity::Epic:      return { 0.75f, 0.30f, 1.00f };
        case Rarity::Legendary: return { 1.00f, 0.75f, 0.20f };
    }
    return { 1.0f, 1.0f, 1.0f };
}

enum class ItemId {
    AutoSyringeRing,
    OrbitalRing,
};

constexpr int kItemIdCount = 2;

struct ItemInstance {
    ItemId id;
    Rarity rarity;
};

inline const char* itemName(ItemId id) {
    switch (id) {
        case ItemId::AutoSyringeRing: return "AUTO RING";
        case ItemId::OrbitalRing:     return "ORBIT RING";
    }
    return "?";
}

inline const char* itemDesc(ItemId id) {
    switch (id) {
        case ItemId::AutoSyringeRing: return "Auto-fires at the nearest enemy.";
        case ItemId::OrbitalRing:     return "Orbits a syringe that damages on touch.";
    }
    return "";
}

}  // namespace game

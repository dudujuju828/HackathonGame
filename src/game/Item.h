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
    HailRing,
    ExplosiveAuto,
    LightningRing,
};

constexpr int kItemIdCount = 5;

struct ItemInstance {
    ItemId id;
    Rarity rarity;
};

inline const char* itemName(ItemId id) {
    switch (id) {
        case ItemId::AutoSyringeRing: return "AUTO RING";
        case ItemId::OrbitalRing:     return "ORBIT RING";
        case ItemId::HailRing:        return "HAIL RING";
        case ItemId::ExplosiveAuto:   return "EXPLOSIVE AUTO";
        case ItemId::LightningRing:   return "LIGHTNING RING";
    }
    return "?";
}

inline const char* itemDesc(ItemId id) {
    switch (id) {
        case ItemId::AutoSyringeRing: return "Auto-fires at the nearest enemy.";
        case ItemId::OrbitalRing:     return "Orbits a syringe that damages on touch.";
        case ItemId::HailRing:        return "Every 5s, rains syringes within 10m.";
        case ItemId::ExplosiveAuto:   return "Auto-fire shots burst on impact.";
        case ItemId::LightningRing:   return "Auto-fire chains to 3 nearest enemies.";
    }
    return "";
}

// Each item is bound to a fixed rarity tier — chests of that rarity grant it.
inline Rarity itemRarity(ItemId id) {
    switch (id) {
        case ItemId::AutoSyringeRing: return Rarity::Common;
        case ItemId::OrbitalRing:     return Rarity::Common;
        case ItemId::HailRing:        return Rarity::Uncommon;
        case ItemId::ExplosiveAuto:   return Rarity::Rare;
        case ItemId::LightningRing:   return Rarity::Rare;
    }
    return Rarity::Common;
}

}  // namespace game

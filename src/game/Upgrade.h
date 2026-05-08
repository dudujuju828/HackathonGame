#pragma once

#include <vector>

namespace game {

enum class UpgradeType {
    MoveSpeed,
    SprintSpeed,
    FireRate,
    MaxHealth,
    ExtraProjectile,
    ProjectileSpeed,
    FanOut,
};

struct Upgrade {
    UpgradeType type;
    const char* name;
    const char* description;
    float       magnitude; // e.g. 1.15f for 15% increase, or flat addition
};

// Returns a pool of all available upgrades.
inline std::vector<Upgrade> getUpgradePool() {
    return {
        { UpgradeType::MoveSpeed,        "ATHLETE",       "+15% Walking Speed", 1.15f },
        { UpgradeType::SprintSpeed,      "ADRENALINE",    "+20% Sprint Speed",  1.20f },
        { UpgradeType::FireRate,         "FAST TRIGGER",  "+25% Fire Rate",     0.75f }, // 0.75 * interval = faster
        { UpgradeType::ProjectileSpeed,  "HYPER-NEEDLE",  "+40% Proj Speed",    1.40f },
        { UpgradeType::ExtraProjectile,  "TWIN NEEDLE",   "+1 Syringe (Burst)", 1.0f  },
        { UpgradeType::FanOut,           "SPREAD SHOT",   "+1 Syringe (Fan)",   1.0f  },
    };
}

}  // namespace game

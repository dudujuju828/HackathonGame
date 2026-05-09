#pragma once

#include "Item.h"

#include <glm/glm.hpp>

#include <vector>

namespace render { class Hud; class Text; }

namespace game {

struct PlayerStats {
    float health        = 100.0f;
    float maxHealth     = 100.0f;
    float damage        = 10.0f;
    float attackSpeed   = 2.0f;
    float stamina       = 20.0f;
    float maxStamina    = 20.0f;
    int   level         = 1;
    float autoRingRate  = 0.0f;  // shots/sec from stacked AutoSyringeRings
    int   orbitalCount  = 0;     // number of stacked OrbitalRings
    int   hailCount     = 0;     // number of stacked HailRings
    int   explosiveAutoCount = 0;// number of stacked ExplosiveAuto
    int   lightningCount = 0;    // number of stacked LightningRings
    std::vector<ItemInstance> items;
};

class StatsScreen {
public:
    void draw(render::Hud& hud, render::Text& text,
              int targetW, int targetH, const PlayerStats& stats) const;
};

}  // namespace game

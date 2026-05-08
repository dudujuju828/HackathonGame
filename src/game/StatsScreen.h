#pragma once

#include <glm/glm.hpp>

namespace render { class Hud; class Text; }

namespace game {

struct PlayerStats {
    float health      = 100.0f;
    float maxHealth   = 100.0f;
    float damage      = 10.0f;
    float attackSpeed = 2.0f;
    float stamina     = 20.0f;
    float maxStamina  = 20.0f;
    int   level       = 1;
};

class StatsScreen {
public:
    void draw(render::Hud& hud, render::Text& text,
              int targetW, int targetH, const PlayerStats& stats) const;
};

}  // namespace game

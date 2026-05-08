#pragma once

#include "Upgrade.h"
#include <glm/glm.hpp>
#include <vector>
#include <optional>

namespace core   { class Input; }
namespace render { class Hud; class Text; }

namespace game {

class LevelUpMenu {
public:
    LevelUpMenu() = default;

    // Call this when entering the LevelUp scene to pick 3 random options.
    void reset();

    // Returns the selected upgrade if the user clicked one this frame.
    std::optional<Upgrade> update(float dt, const core::Input& input,
                                  int targetW, int targetH);

    void draw(render::Hud& hud, render::Text& text,
              int targetW, int targetH);

private:
    struct Rect { float x0, y0, x1, y1; };
    Rect optionRect_(int W, int H, int slotIdx) const;

    std::vector<Upgrade> options_;
    int     hoverIdx_ = -1;
    bool    prevMouse_ = false;
    bool    blockUntilRelease_ = false;
    uint32_t rngState_ = 0x600DBEEFu;

    float rand01_();
};

}  // namespace game

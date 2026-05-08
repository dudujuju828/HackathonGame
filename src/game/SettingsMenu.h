#pragma once

#include "Settings.h"

#include <glm/glm.hpp>

namespace core   { class Input; }
namespace render { class Hud; class Text; }

namespace game {

// Modal in-game settings overlay. Owns the slider drag state and FOV value.
// main constructs one and calls update + draw while in Scene::Settings.
class SettingsMenu {
public:
    void update(float dt, const core::Input& input, int targetW, int targetH);

    // Caller is responsible for binding the destination framebuffer
    // (typically the default after the post-process pass).
    void draw(render::Hud& hud, render::Text& text, int targetW, int targetH);

    Settings&       settings()       { return settings_; }
    const Settings& settings() const { return settings_; }

private:
    // Slider geometry in window pixels — recomputed each frame from screen
    // size so the menu adapts to resizes.
    struct SliderRect { float x0, y0, x1, y1; };
    SliderRect fovTrack_(int W, int H) const;

    Settings settings_ {};
    bool     dragging_  = false;
    bool     prevMouse_ = false;
};

}  // namespace game

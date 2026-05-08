#pragma once

#include "Settings.h"

#include <glm/glm.hpp>

#include <vector>

namespace core   { class Input; }
namespace render { class Hud; class Text; }

namespace game {

// Modal in-game settings overlay. Renders one or more sliders bound to
// fields on Settings; main constructs one and calls update + draw while
// the scene is Settings.
class SettingsMenu {
public:
    void update(float dt, const core::Input& input, int targetW, int targetH);

    // Caller is responsible for binding the destination framebuffer
    // (typically the default after the post-process pass).
    void draw(render::Hud& hud, render::Text& text, int targetW, int targetH);

    Settings&       settings()       { return settings_; }
    const Settings& settings() const { return settings_; }

private:
    struct SliderDef {
        const char* label;
        float*      value;        // points into settings_
        float       min;
        float       max;
        float       yOffsetPx;    // track Y from top of the panel
    };

    struct TrackRect { float x0, y0, x1, y1; };

    void      buildSliders_(std::vector<SliderDef>& out);
    TrackRect trackOf_(int W, int H, const SliderDef& s) const;

    Settings settings_   {};
    int      draggingIdx_ = -1;   // -1 = none
    bool     prevMouse_   = false;
};

}  // namespace game

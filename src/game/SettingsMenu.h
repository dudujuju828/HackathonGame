#pragma once

#include "Settings.h"

#include <glm/glm.hpp>

#include <vector>

namespace core   { class Input; }
namespace render { class Hud; class Text; }

namespace game {

// Discrete actions the menu can hand back to main on a given frame.
enum class MenuAction {
    None,
    ExitGame,
};

class SettingsMenu {
public:
    // Returns any one-shot action triggered this frame (e.g. exit pressed).
    MenuAction update(float dt, const core::Input& input,
                      int targetW, int targetH);

    void draw(render::Hud& hud, render::Text& text,
              int targetW, int targetH);

    Settings&       settings()       { return settings_; }
    const Settings& settings() const { return settings_; }

private:
    struct Item {
        enum class Kind { Slider, Toggle, Button };

        Kind        kind;
        const char* label;
        float       yOffsetPx;     // y position from panel top

        // Slider only.
        float*      sliderValue = nullptr;
        float       sliderMin   = 0.0f;
        float       sliderMax   = 1.0f;

        // Toggle only.
        bool*       toggleValue = nullptr;

        // Button only.
        MenuAction  action      = MenuAction::None;
    };

    struct Rect { float x0, y0, x1, y1; };

    void  buildItems_(std::vector<Item>& out);
    Rect  hitRect_(int W, int H, const Item& it) const;
    Rect  trackRect_(int W, int H, const Item& it) const;

    Settings settings_   {};
    int      draggingIdx_ = -1;   // active slider; -1 = none
    bool     prevMouse_   = false;
};

}  // namespace game

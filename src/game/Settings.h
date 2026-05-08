#pragma once

namespace game {

// User-controlled gameplay/render settings.
// Settings carries only current values; the Settings menu owns slider
// ranges so they don't need to live in the data model.
struct Settings {
    float hFovDeg       = 100.0f;  // horizontal FOV (deg)
    float flashlightDeg = 20.0f;   // flashlight outer-cone half-angle (deg)
};

}  // namespace game

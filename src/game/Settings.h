#pragma once

namespace game {

// User-controlled gameplay/render settings.
// Settings carries only current values; the Settings menu owns slider
// ranges so they don't need to live in the data model.
struct Settings {
    float hFovDeg       = 116.0f;  // horizontal FOV (deg)
    float flashlightDeg = 45.0f;   // flashlight outer-cone half-angle (deg)
    bool  fullscreen    = true;
    int   mapIndex      = 0;

    static constexpr int kMapCount = 3;
    static constexpr const char* kMapNames[kMapCount] = {
        "Hollow Creek",
        "Ashwood",
        "The Mire",
    };
};

}  // namespace game

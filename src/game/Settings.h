#pragma once

namespace game {

// User-controlled gameplay/render settings.
struct Settings {
    float hFovDeg     = 100.0f;  // horizontal FOV, slider range 70..130
    float hFovMinDeg  = 70.0f;
    float hFovMaxDeg  = 130.0f;
};

}  // namespace game

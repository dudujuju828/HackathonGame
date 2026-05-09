#pragma once

#include <glm/glm.hpp>

namespace game {

constexpr float kAntidotePickupRadius = 3.0f;   // metres; how close to auto-collect
constexpr float kAntidoteScale        = 0.5f;   // uniform draw scale
constexpr float kAntidoteYOffset      = 1.2f;    // hover height above terrain
constexpr float kAntidoteSpawnDist    = 40.0f;   // metres from player at wave start
constexpr float kAntidoteBobAmp       = 0.35f;   // vertical hover amplitude
constexpr float kAntidoteBobFreq      = 0.8f;    // hz

enum class AntidoteBoxState {
    Active,
    Collected,
};

struct AntidoteBox {
    glm::vec3        position { 0.0f };
    AntidoteBoxState state    = AntidoteBoxState::Active;
    float            bobTimer = 0.0f;
    float            yawRad   = 0.0f;
};

}  // namespace game

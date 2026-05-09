#pragma once

#include <glm/glm.hpp>

namespace game {

constexpr float kAntidotePickupRadius = 3.0f;   // metres; how close to auto-collect
constexpr float kAntidoteScale        = 0.5f;   // uniform draw scale
constexpr float kAntidoteYOffset      = 0.5f;    // metres above terrain
constexpr float kAntidoteSpawnDist    = 40.0f;   // metres from player at wave start
constexpr float kAntidoteBasePitchDeg = 0.0f;    // X-rotation fix for Z-up GLB

enum class AntidoteBoxState {
    Active,
    Collected,
};

struct AntidoteBox {
    glm::vec3        position { 0.0f };
    AntidoteBoxState state    = AntidoteBoxState::Active;
    float            yawRad   = 0.0f;
};

}  // namespace game

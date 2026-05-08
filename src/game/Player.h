#pragma once

#include "render/Camera.h"

#include <glm/glm.hpp>

namespace core { class Input; }

namespace game {

struct WalkFeel {
    // Movement
    float moveSpeed   = 4.0f;     // m/s
    float headHeight  = 1.6f;     // m

    // Step bob — head bobs vertically every step, sways laterally every other.
    float bobAmpY     = 0.045f;   // m
    float bobAmpX     = 0.025f;   // m
    float bobStepRate = 0.35f;    // steps per meter walked (slow, horror cadence)
    float bobBlend    = 8.0f;     // 1/s smoothing for fade in/out

    // Idle view shake — tiny constant breath wobble on yaw/pitch.
    float idleShakeYaw   = 0.08f;  // degrees amplitude
    float idleShakePitch = 0.05f;  // degrees amplitude
    float shakeFreqA     = 1.7f;   // Hz
    float shakeFreqB     = 2.3f;   // Hz

    // Trauma-driven view shake (added to idle). Use addTrauma() on impacts,
    // gunfire, etc.; magnitude decays automatically.
    float traumaDecay         = 1.5f;  // per second
    float traumaShakeYawMax   = 1.5f;  // degrees at trauma=1
    float traumaShakePitchMax = 1.0f;  // degrees at trauma=1
};

class Player {
public:
    Player();

    void setSpawn(const glm::vec3& feetPosition);
    void update(float dt, const core::Input& input, float timeSeconds);

    // Add to the trauma accumulator (clamped 0..1). Squared in-shader for
    // a perceptual decay curve.
    void addTrauma(float amount);

    glm::vec3 position() const { return position_; }
    glm::vec3 velocity() const { return velocity_; }

    render::Camera&       camera()       { return camera_; }
    const render::Camera& camera() const { return camera_; }

    WalkFeel&       feel()       { return feel_; }
    const WalkFeel& feel() const { return feel_; }

private:
    glm::vec3 position_ { 0.0f, 0.0f, 3.0f };  // feet
    glm::vec3 velocity_ { 0.0f };

    float bobPhase_     = 0.0f;
    float bobIntensity_ = 0.0f;  // 0..1, smoothed
    float trauma_       = 0.0f;  // 0..1

    WalkFeel       feel_   {};
    render::Camera camera_ {};
};

}  // namespace game

#include "Player.h"

#include "core/Input.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace game {

namespace { constexpr float TWO_PI = 6.28318530718f; }

Player::Player() {
    setSpawn({ 0.0f, 0.0f, 3.0f });
}

void Player::setSpawn(const glm::vec3& feet) {
    position_        = feet;
    velocity_        = glm::vec3(0.0f);
    bobPhase_        = 0.0f;
    bobIntensity_    = 0.0f;
    trauma_          = 0.0f;
    camera_.position = feet + glm::vec3(0.0f, feel_.headHeight, 0.0f);
    camera_.viewShakeYaw   = 0.0f;
    camera_.viewShakePitch = 0.0f;
}

void Player::addTrauma(float a) {
    trauma_ = std::clamp(trauma_ + a, 0.0f, 1.0f);
}

void Player::update(float dt, const core::Input& input, float t) {
    // Mouse look (advances yaw/pitch on the camera).
    camera_.addLook(input.mouseDX(), input.mouseDY());

    // Movement basis: pure yaw, no pitch and no view-shake.
    const float yawRad = glm::radians(camera_.yaw);
    const glm::vec3 fwd  { std::cos(yawRad), 0.0f, std::sin(yawRad) };
    const glm::vec3 side = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));

    glm::vec3 wish(0.0f);
    if (input.key(GLFW_KEY_W)) wish += fwd;
    if (input.key(GLFW_KEY_S)) wish -= fwd;
    if (input.key(GLFW_KEY_D)) wish += side;
    if (input.key(GLFW_KEY_A)) wish -= side;
    if (glm::length(wish) > 1e-4f) wish = glm::normalize(wish);

    velocity_ = wish * feel_.moveSpeed;
    position_ += velocity_ * dt;

    // Step phase advances with ground speed.
    const float speed = glm::length(velocity_);
    bobPhase_ += speed * feel_.bobStepRate * dt;
    if (bobPhase_ > 1e6f) bobPhase_ = std::fmod(bobPhase_, 1.0f);

    // Smooth bob amplitude in/out so stopping doesn't snap.
    const float target = (speed > 0.05f) ? 1.0f : 0.0f;
    const float blend  = std::min(feel_.bobBlend * dt, 1.0f);
    bobIntensity_ = glm::mix(bobIntensity_, target, blend);

    // Trauma decay.
    trauma_ = std::max(0.0f, trauma_ - feel_.traumaDecay * dt);

    // Compose head position: feet + head + bob.
    glm::vec3 head = position_ + glm::vec3(0.0f, feel_.headHeight, 0.0f);
    const float bobY = std::sin(bobPhase_ * TWO_PI)        * feel_.bobAmpY * bobIntensity_;
    const float bobX = std::sin(bobPhase_ * TWO_PI * 0.5f) * feel_.bobAmpX * bobIntensity_;
    head += glm::vec3(0.0f, 1.0f, 0.0f) * bobY + side * bobX;
    camera_.position = head;

    // View shake — idle breath + trauma spikes (perceptual quadratic).
    const float idleY = std::sin(t * feel_.shakeFreqA * TWO_PI) * feel_.idleShakeYaw;
    const float idleP = std::sin(t * feel_.shakeFreqB * TWO_PI) * feel_.idleShakePitch;

    const float k     = trauma_ * trauma_;
    const float trY   = std::sin(t * 41.0f) * k * feel_.traumaShakeYawMax;
    const float trP   = std::cos(t * 37.0f) * k * feel_.traumaShakePitchMax;

    camera_.viewShakeYaw   = idleY + trY;
    camera_.viewShakePitch = idleP + trP;
}

}  // namespace game

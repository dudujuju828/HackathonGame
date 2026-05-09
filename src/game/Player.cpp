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

void Player::clampXZ(float minX, float maxX, float minZ, float maxZ) {
    position_.x = std::clamp(position_.x, minX, maxX);
    position_.z = std::clamp(position_.z, minZ, maxZ);
    camera_.position.x = position_.x;
    camera_.position.z = position_.z;
}

void Player::setSpawn(const glm::vec3& feet) {
    position_        = feet;
    velocity_        = glm::vec3(0.0f);
    knockbackVel_    = glm::vec3(0.0f);
    bobPhase_        = 0.0f;
    bobIntensity_    = 0.0f;
    trauma_          = 0.0f;
    onGround_        = true;
    prevJump_        = false;
    camera_.position = feet + glm::vec3(0.0f, feel_.headHeight, 0.0f);
    camera_.viewShakeYaw   = 0.0f;
    camera_.viewShakePitch = 0.0f;
}

void Player::restart() {
    health      = 100.0f;
    maxHealth   = 100.0f;
    damage      = 10.0f;
    attackSpeed = 2.0f;
    stamina     = 20.0f;
    maxStamina  = 20.0f;
    inventory_.clear();
    feel_            = WalkFeel{};
    level_           = 1;
    xp_              = 0;
    xpToNext_        = 100;
    pendingLevelUps_ = 0;
    setSpawn({ 0.0f, 0.0f, 3.0f });
}

void Player::addTrauma(float a) {
    trauma_ = std::clamp(trauma_ + a, 0.0f, 1.0f);
}

void Player::applyKnockback(const glm::vec3& dir) {
    glm::vec3 d(dir.x, 0.0f, dir.z);
    const float len = std::sqrt(d.x * d.x + d.z * d.z);
    if (len < 1e-4f) return;
    knockbackVel_ = (d / len) * feel_.knockbackStrength;
}

void Player::addXp(int amount) {
    if (amount <= 0) return;
    xp_ += amount;
    while (xp_ >= xpToNext_) {
        xp_       -= xpToNext_;
        level_    += 1;
        xpToNext_  = 100 * level_;  // linear ramp; swap for a curve later
        pendingLevelUps_ += 1;
    }
}

void Player::setProgress(int level, int xp) {
    level_     = std::max(1, level);
    xp_        = std::max(0, xp);
    xpToNext_  = 100 * level_;
}

int Player::countItem(ItemId id) const {
    int n = 0;
    for (const auto& it : inventory_) if (it.id == id) ++n;
    return n;
}

float Player::autoRingRate() const {
    // Each ring contributes 1 shot/sec; rarity is cosmetic for now.
    return static_cast<float>(countItem(ItemId::AutoSyringeRing));
}

void Player::update(float dt, const core::Input& input, float t, float groundHeight) {
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

    const bool wantsSprintKey =
        input.key(GLFW_KEY_LEFT_SHIFT) || input.key(GLFW_KEY_RIGHT_SHIFT);
    const bool sprint = wantsSprintKey && stamina > 0.0f;

    if (sprint) {
        stamina = std::max(0.0f, stamina - 4.0f * dt);
        staminaRegenTimer_ = 2.0f;
    } else {
        if (staminaRegenTimer_ > 0.0f) {
            staminaRegenTimer_ -= dt;
        } else {
            stamina = std::min(maxStamina, stamina + 10.0f * dt);
        }
    }

    const float horizSpeed =
        feel_.moveSpeed * (sprint ? feel_.sprintMultiplier : 1.0f);

    // Vertical: integrate gravity, then optionally override on jump.
    velocity_.y -= feel_.gravity * dt;

    const bool jumpNow         = input.key(GLFW_KEY_SPACE);
    const bool jumpJustPressed = jumpNow && !prevJump_;
    prevJump_ = jumpNow;
    if (jumpJustPressed && onGround_) {
        velocity_.y = feel_.jumpVelocity;
        onGround_   = false;
    }

    velocity_.x = wish.x * horizSpeed;
    velocity_.z = wish.z * horizSpeed;
    position_ += (velocity_ + knockbackVel_) * dt;
    knockbackVel_ *= std::exp(-feel_.knockbackDecay * dt);

    // Clamp feet to terrain surface.
    if (position_.y <= groundHeight) {
        position_.y = groundHeight;
        if (velocity_.y < 0.0f) velocity_.y = 0.0f;
        onGround_ = true;
    }

    // Step phase advances with ground speed (xz only).
    const float speedXZ = std::sqrt(velocity_.x * velocity_.x +
                                    velocity_.z * velocity_.z);
    bobPhase_ += speedXZ * feel_.bobStepRate * dt;
    if (bobPhase_ > 1e6f) bobPhase_ = std::fmod(bobPhase_, 1.0f);

    // Bob only while moving on the ground; mute mid-air so the head doesn't
    // bob during a jump.
    const float target = (speedXZ > 0.05f && onGround_) ? 1.0f : 0.0f;
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

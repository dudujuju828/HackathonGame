#pragma once

#include "Item.h"
#include "render/Camera.h"

#include <glm/glm.hpp>

#include <vector>

namespace core { class Input; }

namespace game {

struct WalkFeel {
    // Movement
    float moveSpeed        = 4.0f;   // m/s walking baseline
    float sprintMultiplier = 1.7f;   // multiplied with moveSpeed while sprinting
    float headHeight       = 1.6f;   // m

    // Vertical
    float gravity      = 25.0f;      // m/s^2
    float jumpVelocity = 7.5f;       // m/s; with gravity 25 -> ~1.13 m peak / 0.60 s hang

    // Step bob — head bobs vertically every step, sways laterally every other.
    float bobAmpY     = 0.075f;   // m
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
    void update(float dt, const core::Input& input, float timeSeconds, float groundHeight = 0.0f);

    // Clamp the player's XZ position to the given world rectangle (used to
    // keep the player inside the perimeter walls). The camera's horizontal
    // position is updated to match so the view doesn't lag by a frame.
    void clampXZ(float minX, float maxX, float minZ, float maxZ);

    // Add to the trauma accumulator (clamped 0..1). Squared in-shader for
    // a perceptual decay curve.
    void addTrauma(float amount);

    glm::vec3 position() const { return position_; }
    glm::vec3 velocity() const { return velocity_; }

    // Progression. addXp handles the level-up loop; setProgress is for
    // initial state (debug, save-game restore, etc.).
    int  level()    const { return level_; }
    int  xp()       const { return xp_; }
    int  xpToNext() const { return xpToNext_; }
    void addXp(int amount);
    void setProgress(int level, int xp);

    int  pendingLevelUps() const { return pendingLevelUps_; }
    void consumeLevelUp()        { if (pendingLevelUps_ > 0) pendingLevelUps_--; }

    // Items are passive and permanent — no remove. Duplicates stack: e.g.
    // two AutoSyringeRings means 2 shots/sec instead of 1.
    void grantItem(const ItemInstance& item) { inventory_.push_back(item); }
    const std::vector<ItemInstance>& inventory() const { return inventory_; }
    int  countItem(ItemId id) const;

    // Full reset to initial state — called on game-over restart.
    void restart();
    // Per-second auto-fire rate from all stacked AutoSyringeRings.
    float autoRingRate() const;

    render::Camera&       camera()       { return camera_; }
    const render::Camera& camera() const { return camera_; }

    WalkFeel&       feel()       { return feel_; }
    const WalkFeel& feel() const { return feel_; }

    // Base combat/progression stats — modified by upgrades and abilities.
    float health      = 100.0f;
    float maxHealth   = 100.0f;
    float damage      = 10.0f;    // damage per syringe
    float attackSpeed = 2.0f;     // syringes per second
    float stamina     = 20.0f;
    float maxStamina  = 20.0f;

private:
    glm::vec3 position_ { 0.0f, 0.0f, 3.0f };  // feet
    glm::vec3 velocity_ { 0.0f };

    float bobPhase_          = 0.0f;
    float bobIntensity_      = 0.0f;  // 0..1, smoothed
    float trauma_            = 0.0f;  // 0..1
    float staminaRegenTimer_ = 0.0f;  // seconds remaining before regen starts
    bool  onGround_          = true;
    bool  prevJump_          = false;

    int level_     = 1;
    int xp_        = 0;
    int xpToNext_  = 100;
    int pendingLevelUps_ = 0;

    std::vector<ItemInstance> inventory_;

    WalkFeel       feel_   {};
    render::Camera camera_ {};
};

}  // namespace game

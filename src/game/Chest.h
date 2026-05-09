#pragma once

#include "Item.h"
#include "render/Animator.h"

#include <glm/glm.hpp>

namespace game {

constexpr float kChestOpenDuration = 3.0f;    // seconds for the open animation + colour cycle
constexpr float kChestPickupRadius = 2.5f;    // metres; how close the player needs to be to press E
constexpr float kChestScale        = 0.0048f; // uniform scale at draw time
constexpr float kChestYOffset      = 0.0f;    // model origin already sits at the floor for this GLB
constexpr float kChestBasePitchDeg = -90.0f;  // rotate around X so the lid faces up (model ships Z-up)
constexpr float kChestDropChance   = 0.20f;   // probability per basic-enemy kill
constexpr float kChestFadeDuration = 0.8f;    // seconds — how long the chest fades after opening

enum class ChestState {
    Closed,
    Opening,   // playing the colour cycle + chest animation
    Fading,    // cycle finished, popup shown, chest fades from view
    Done,      // marked for cull
};

struct Chest {
    glm::vec3   position { 0.0f };
    float       yawRad     = 0.0f;            // random facing
    Rarity      rolled     = Rarity::Common;  // pre-rolled at spawn time
    ItemId      itemId     = ItemId::AutoSyringeRing;
    ChestState  state      = ChestState::Closed;
    float       openTimer  = 0.0f;            // 0..kChestOpenDuration while Opening
    float       fadeTimer  = 0.0f;            // 0..kChestFadeDuration while Fading
    float       cyclePhase = 0.0f;            // accumulator that drives random tint swaps
    glm::vec3   cycleTint { 1.0f, 1.0f, 1.0f };
    float       emitTimer  = 0.0f;            // particle emission accumulator

    render::Animator animator;

    // Tint to apply to the chest model this frame. Random rarity flickers
    // while opening, snaps to the rolled colour for fade/done.
    glm::vec3 currentTint() const;
    // 1.0 except during Fading, where it ramps to 0 over kChestFadeDuration.
    float     currentAlpha() const;
};

// Roll a rarity using the loot table (50/25/15/8/2).
Rarity rollChestRarity();

// Pick a uniformly random item from the available pool.
ItemId rollChestItem();

}  // namespace game

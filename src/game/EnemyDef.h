#pragma once

namespace render {
class  Model;
struct AnimationClip;
}

namespace game {

// Immutable per-type data shared across every enemy of the same kind.
// All instances hold a const pointer into a table allocated at startup.
struct EnemyDef {
    const char*                  name     = "?";    // referenced from waves.txt
    render::Model*               model    = nullptr;
    const render::AnimationClip* walkAnim = nullptr;
    float scale         = 0.30f;  // uniform GLB scale at draw time
    float height        = 1.70f;  // Y lift so feet touch the floor; also the hit-sphere centre
    float radius        = 0.60f;  // collision sphere radius (world units)
    float bobFreq       = 0.0f;   // procedural walk-bob frequency (rad/s); 0 = disabled
    float bobAmp        = 0.0f;   // procedural walk-bob amplitude (world units)
    float basePitchDeg  = 0.0f;   // fix-up rotation around the model's local X (use -90 for Z-up GLBs)
    float baseYawDeg    = 0.0f;   // fix-up heading rotation around Y, applied after basePitch (use to make the head face forward)
    float baseRollDeg   = 0.0f;   // side-lean around the model's final forward axis (post chase-yaw)
    int   maxHp         = 2;      // hp this enemy spawns with
    float dropChance    = 0.10f;  // 0..1 — probability this enemy drops a chest on death
    float moveSpeed     = 1.3f;   // m/s chase speed
    float damage        = 5.0f;   // hp dealt to player per hit
    float attackRange   = 1.6f;   // metres — must be within this to land a hit
    float attackInterval = 1.0f;  // seconds between this enemy's hits

    // Aggro mode: when the player is within aggroRange, swap to aggroAnim
    // and multiply moveSpeed by aggroSpeedMult. nullptr disables (default).
    const render::AnimationClip* aggroAnim = nullptr;
    float aggroRange     = 0.0f;  // metres
    float aggroSpeedMult = 1.0f;  // multiplier on moveSpeed in aggro

    // Death animation. When set, the enemy plays this clip on death and
    // fades from 1.0 to 0.0 alpha across its second half before being
    // culled. nullptr = legacy behaviour (instant removal on death).
    const render::AnimationClip* deathAnim = nullptr;

    // Optional positional one-shot fired when this enemy lands a hit on
    // the player. nullptr = silent attack. Path is relative to the exe's
    // working directory; the audio system uses miniaudio's resource
    // manager so repeated paths decode once.
    const char* attackSound = nullptr;

    // Boss-only data. When `isBoss` is true, the enemy bypasses the wave
    // HP multiplier (its `maxHp` is used verbatim) and the rendering /
    // cutscene code can locate the active boss without a separate registry.
    // `roarAnim` is forced as a one-shot during the boss intro cutscene
    // before the normal aggro/walk switching takes over. `attackAnim`
    // plays as a one-shot whenever the enemy is inside `attackRange` and
    // there isn't already an attack animation in flight. Kept at the end
    // of the struct so existing rows that don't supply these stay valid.
    bool                         isBoss     = false;
    const render::AnimationClip* roarAnim   = nullptr;
    const render::AnimationClip* attackAnim = nullptr;
};

} // namespace game

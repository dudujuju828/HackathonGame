#pragma once

namespace render {
class  Model;
struct AnimationClip;
}

namespace game {

// Immutable per-type data shared across every enemy of the same kind.
// All instances hold a const pointer into a table allocated at startup.
struct EnemyDef {
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
};

} // namespace game

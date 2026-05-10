# HackathonGame — Technical Feature Overview

A wave-based FPS horror built from scratch in C++20 with a hand-rolled OpenGL 3.3 renderer.
No Unity. No Unreal. No game engine.

---

## Engine Architecture

The engine is split into three clean namespaces: `core/`, `render/`, and `game/`. The
rendering layer knows nothing about gameplay — game code submits data and the renderer
draws it. No coupling runs backwards. All GPU resources (`Shader`, `Texture`, `Mesh`,
`Framebuffer`, `Model`, `Glow`, `Particles`) are move-only RAII classes; there are zero raw
GL handles living outside `render/`. Every resource cleans itself up in its destructor.
The architecture would be recognisable to an engineer on a shipping game team.

**~70 source files, 20 GLSL shaders, fully hand-authored.**

---

## Skeletal Animation Pipeline

The most technically demanding system in the codebase, implemented entirely from scratch.

### Skeleton & Joint Hierarchy
Each animated enemy carries a `Skeleton` — a flat array of `Joint` entries ordered so
parents always precede children. Every joint stores its parent index, inverse bind matrix
(for linear blend skinning), and rest-pose TRS decomposed into translation (vec3),
rotation (quaternion), and scale (vec3).

### Keyframe Interpolation
Animation clips store per-joint `AnimationChannel`s — parallel arrays of time-stamped
keyframes. At runtime the animator binary-searches for the bracketing pair and interpolates:

- **Translation / Scale**: `glm::mix` (lerp) between keyframes
- **Rotation**: `glm::slerp` for smooth quaternion blending with no gimbal lock
- **Edge cases handled**: empty channels default to rest pose; single keyframes repeat;
  time past the last keyframe clamps to hold the final pose; looping via `fmod`.
- **One-shot support**: death animations play to completion then the enemy fades out

### Linear Blend Skinning (LBS)
Each vertex carries up to 4 `(boneId, weight)` influences. The world vertex shader
accumulates `sum(uBones[id[i]] * weight[i])` across all influences and applies the
blended transform to both position and normal. The uniform array supports up to 200 bone
matrices — raised from 100 specifically to handle the Cat GLB's 175-joint rig.

### Bone Transform Computation
Each frame the `Animator` traverses the skeleton in hierarchy order, composing local
`translate * rotate * scale` matrices from animated TRS, then accumulating
`global = parent_global * local`, then computing the final skinning matrix as
`global * inverseBindMatrix`. This is a complete, correct implementation of the standard
game-industry skeletal animation algorithm.

### Procedural Animation Synthesis
Two enemies — Bulldog and Cat — have no baked walk animations in their GLB files.
Their locomotion is synthesised at runtime:

- **Bulldog** (38-joint skeleton): Diagonal gait — front-left and hind-right legs move in
  phase; front-right and hind-left move in anti-phase. Each leg swings ±30–35° around its
  local X axis via a sine oscillator. The spine sways at 2× stride frequency; the root
  bounces vertically. Stride period: 0.7 s.
- **Cat** (28-joint skeleton): Same diagonal gait at 0.95 s per stride. Tail base swishes
  around local Z in sync with footfalls; spine adds a subtle lateral sway for a feline
  quality.

Both procedures write directly into `AnimationClip` keyframe arrays at startup, then the
normal interpolation pipeline plays them back — no special-case code at draw time.

### GLB Import Pipeline
All animated enemies are loaded from binary glTF (GLB) via **cgltf**. The loader extracts
mesh geometry, PBR materials, embedded textures, skeleton (skin), and animation clips in
one pass. Notable correctness details:

- cgltf delivers quaternions as `(x, y, z, w)`; GLM expects `(w, x, y, z)` — converted at
  load time for every joint and every rotation keyframe
- glTF uses top-left UV origin; OBJ uses bottom-left — stb_image V-flip is disabled for
  glTF and enabled for OBJ so all UVs normalise to the same convention
- Per-model axis fix-up rotations (`basePitchDeg`, `baseYawDeg`, `baseRollDeg`) correct
  heterogeneous import orientations without touching the asset files

---

## Rendering Pipeline

### Forward Shading Pass
The world shader implements Blinn-Phong lighting driven by the player's flashlight:

```
lit = albedo * ambient + albedo * flashColor * coneMask * attenuation * NdotL
```

The cone mask is a smooth step between inner and outer cosine angles; attenuation is
inverse-square. Both skeletal (LBS via bone matrix array) and rigid (identity bone array)
geometry go through the same shader — no shader switching per draw.

### HDR Skybox
A fullscreen-quad skybox reconstructs view rays from clip space using the inverse
view-projection matrix, converts to equirectangular UV, samples an HDR texture stored as
`GL_RGB16F`, and applies Reinhard tonemapping to prevent blown-out highlights. Zero
geometry submitted — pure fragment shader math.

### CRT Post-Processing Shader
A full-screen pass applies five simultaneous effects in a single `post_crt.frag` draw:

1. **Barrel distortion** — warps UVs outward from centre; outside the curved screen is a
   black bezel. Mathematically: distort(uv) = uv + (uv - 0.5) × r² × curvature
2. **Chromatic aberration** — red channel samples shifted outward, blue channel shifted
   inward. Different RGB convergence points simulate a CRT's three-gun phosphor paths.
3. **Scanlines** — virtual scan-line count independent of window resolution (e.g., 280
   lines regardless of 1080p or 4K)
4. **RGB phosphor mask** — per-pixel tint based on screen-space X modulo 3 (red / green /
   blue column columns), simulating real phosphor dot pitch
5. **Animated noise + flicker** — hash-based per-frame grain, a low-frequency luminance
   wobble from two beating sine waves, and occasional sharp horizontal tear bands

All five parameters are live-tunable via a `CrtParams` struct.

### SDF Crosshair
The crosshair is rendered by a dedicated GLSL shader using a signed-distance field: the
shape is the union of two axis-aligned boxes (the plus sign arms), clamped by a circular
gap at centre. Smooth-step antialiasing gives pixel-perfect edges at any DPI with no
texture or rasterised bitmap.

### HUD Bars
The ammo display is a segmented bar rendered in GLSL — the shader knows the weapon
capacity and draws individual cell gaps procedurally. The health bar pulses its fill
colour when HP drops below 30% (a sine mix between normal and warning colour), also
computed in the fragment shader.

### Additive Effects
Chest auras use a `glow_disc` shader: a ground-plane quad with a power-law radial
falloff, drawn with additive blending. Particle point sprites use `gl_PointSize`
perspective-divided by clip-space W so they scale correctly with distance; circular soft
edges via `gl_PointCoord` distance.

### Framebuffer Architecture
All scene geometry renders to an offscreen `Framebuffer` (RGBA8 colour + depth/stencil
renderbuffer). The CRT shader samples that texture and outputs to the default framebuffer.
HUD, text, and minimap draw on top in screen space after the post-FX pass.

---

## Procedural Terrain

The terrain is a 129×129 vertex heightmap (128 quad tiles, 128 m world extent) generated
entirely at runtime:

- **Perlin noise**: Ken Perlin's reference 2D algorithm with 512-entry permutation table,
  fade curve t³(6t²-15t+10), and 8-direction gradient hash
- **Four-biome blending**: The world is divided into quadrant zones (Forest SW, Graveyard
  SE, Default NW, Asylum NE). Each zone runs a 3-octave noise stack tuned independently
  (Asylum: 22 m amplitude, low-frequency ridges; Graveyard: 6 m flat bumps; Forest: 14 m
  rolling canopy). Bilinear weights interpolate between zones — no hard seams
- **Bilinear height query**: Any XZ position can query terrain height via bilinear
  interpolation over the four surrounding grid corners — used for player ground contact,
  enemy positioning, and decoration placement
- **Normal computation**: Central finite differences (one-sided at edges) produce smooth
  per-vertex normals without cross products per triangle

---

## Wave System

Waves are defined in a plain-text `assets/waves.txt` and parsed at startup — no
recompile needed to tune difficulty:

```
WAVE "First Contact: Calici" 30
GROUP  0   Cat 8 Chicken 4
GROUP  6   Chicken 8
GROUP 14   Cat 10 Chicken 6
```

The parser handles quoted wave names, whitespace-separated tokens, and comment lines.
`WaveManager` tracks elapsed time, fires groups at their scheduled `at_sec` offsets, and
gates wave advancement behind antidote collection — forcing the player to explore. The
final 10th wave is a boss-density swarm. HP scaling is applied to all newly spawned
enemies as the player's level increases, keeping difficulty tight.

---

## Enemy Design

Five distinct enemy types, each with its own skeletal rig, tuned stats, and behaviour:

| Enemy    | HP | Speed | Damage | Loot | Technical note |
|----------|----|-------|--------|------|----------------|
| Harpy    |  8 | 2.4 m/s |  12  | 20% | Real glTF fly animation |
| Bulldog  | 20 | 1.7 m/s |  22  | 50% | Procedural 38-joint diagonal gait |
| Cat      |  2 | 3.2→6.4 m/s |  6 | 5%  | Aggro at 9 m: switches clip + 2× speed |
| Pig      | 14 | 2.0 m/s |  16  | 30% | Real glTF walk animation |
| Chicken  |  4 | 2.7 m/s |   7  | 10% | No skeleton; procedural 16 Hz bob + lean |

The Cat's aggro system: when the player enters 9 m, the animator swaps from the walk clip
to the run clip and `speedMult` doubles. At range it stalks; up close it sprints.

---

## Player & Combat Systems

### First-Person Controller
- Walk bob: 0.075 m vertical + 0.025 m lateral per step, blended in/out with movement
- Breath idle: Dual-frequency sway (1.7 + 2.3 Hz) at sub-degree amplitude for an organic
  stationary feel
- Sprint + stamina drain/regen
- Trauma-driven screen shake: Each gunshot adds 0.45 to a 0–1 trauma accumulator that
  decays at 1.5/s. Camera shake magnitude = trauma², giving perceptual smoothing (barely
  noticeable at low trauma; violent at high)

### Weapon System
- Pistol (12-round magazine) and shotgun (8-round magazine)
- Fan spread: multiple syringes fired simultaneously in an arc (`fanColumns`, configurable
  spread angle)
- Burst mode: multiple projectiles per trigger pull with staggered timing
- View-model: inward roll, position bob synced to walk, kickback recoil animation

### Progression
- XP → level-up menu: player picks one of three randomly drawn upgrades
  (MoveSpeed, SprintSpeed, FireRate, MaxHealth, ExtraProjectile, ProjectileSpeed, FanOut)
- Inventory screen (B key): displays collected items
- Stats screen: live readout of every derived combat stat

### Item System
Five item types across five rarity tiers (2% Legendary → 50% Common):

| Item | Rarity | Effect |
|------|--------|--------|
| AutoSyringeRing | Common | Auto-fires at nearest enemy |
| OrbitalRing | Common | Orbits a projectile as a shield |
| HailRing | Uncommon | Rains projectiles in 10 m every 5 s |
| ExplosiveAuto | Rare | Projectiles burst on impact (AoE) |
| LightningRing | Rare | Auto-fire chains to 3 nearest enemies |

Items stack additively; the loot system rolls a five-tier probability table per chest.

### Chest System
Enemies drop chests (configurable drop chance per type). Each chest runs a mini state
machine: `Closed → Opening (3 s, flickers rarity colours + opens) → Fading (0.8 s alpha
fade) → Done`. During opening, a CPU particle emitter runs in sync. The final rarity
colour snap communicates the rolled tier before the toast appears.

---

## Audio

Spatial audio powered by **miniaudio** — sound effects for gunfire, enemy hits, and
ambient atmosphere.

---

## Software Engineering Highlights

- **RAII everywhere**: every GPU handle has exactly one owner; destructors call
  `glDelete*`. No leaks, no double-frees.
- **Data-driven design**: enemy stats in an `EnemyDef` table; wave scripts in a text file;
  CRT parameters in a struct; all tunables near the top of their translation unit — no
  magic numbers buried in logic.
- **Zero coupling render↔game**: `render/` has no `#include` of any `game/` header.
  Game submits plain data; renderer draws it.
- **Move semantics on all resources**: `Model`, `Mesh`, `Texture`, `Framebuffer` delete
  the copy constructor and implement move so they can be stored in `std::vector` and
  transferred without copying GPU state.
- **Single mesh vertex deduplication**: The OBJ loader deduplicates `(position, normal,
  uv)` triplets using an `unordered_map` with a hand-rolled hash at load time.
- **MAX_BONES=200**: Discovered that Cat.glb has a 175-joint rig mid-development; the
  uniform array was extended and recompiled without touching any other system.

---

## Scale Summary

| Metric | Count |
|--------|-------|
| Source files | ~70 |
| GLSL shaders | 20 |
| Enemy types | 5 |
| Animated joints (max) | 175 (Cat) |
| Waves | 10 |
| Item types | 5 |
| Rarity tiers | 5 |
| Biomes | 4 |
| Terrain vertices | 16 641 |
| Upgrade types | 7 |

Built in three days at a hackathon. No engine. No shortcuts on the rendering or animation
math.

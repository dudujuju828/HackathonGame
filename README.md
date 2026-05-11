# HackathonGame

A wave-based first-person horror shooter built from scratch in C++20 on a
custom OpenGL 3.3 renderer. Originally a 3-day hackathon, the renderer and
game systems have grown well past the initial cut.

You play a veterinary specialist deployed to **Containment Zone Alpha-7**,
where an unknown pathogen has driven the local wildlife rabid. Ten antidote
caches are scattered across the map. Survive ten escalating waves, secure
the caches, and don't get cornered by the Tyranno.

## Status

Playable end-to-end:

- Drop-in cutscene → start menu briefing → ten named waves → boss encounter.
- Five fully skeletal-animated enemy types plus a boss (Tyranno).
- XP / level-up upgrade picks (3-of-pool), looted item drops with five
  rarity tiers, five passive item rings that stack additively.
- HUD with health/stamina bars, crosshair, wave timer, minimap (compact +
  `M` to enlarge), CRT post-processing, HDR night skybox.
- Positional audio: boss roars/stomps, cat hisses, ambient layer, footsteps,
  pickup/level-up cues, briefing voice-over.

## Build

Requires:

- CMake ≥ 3.20
- A C++20 compiler (tested with MinGW GCC 14.2 and MSVC 2022)
- Python 3 with `jinja2` (used by GLAD2's generator on first configure):
  `python -m pip install jinja2`

```sh
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --parallel

build/HackathonGame.exe
```

`assets/` and `data/` are auto-copied next to the executable on every
build, so relative paths in code (`assets/shaders/world.vert`,
`data/Journal.csv`, etc.) resolve from the build directory.

All third-party dependencies are pulled in automatically via CMake
`FetchContent` — no package manager, no submodules:

| Library | Version | Purpose |
| --- | --- | --- |
| GLFW | 3.4 | Window + input |
| GLAD2 | 2.0.6 | OpenGL 3.3 core loader |
| GLM | 1.0.1 | Math (vec/mat/quat) |
| stb_image | master | Texture / HDR decoding |
| tinyobjloader | 2.0.0rc13 | OBJ + MTL parsing |
| cgltf | 1.14 | GLB / glTF (skinned meshes + animations) |
| miniaudio | 0.11.21 | Audio engine + resource manager |

## Controls

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` | Move |
| `Shift` | Sprint (drains stamina) |
| `Space` | Jump |
| Mouse | Look |
| Left Mouse | Fire syringe |
| `B` | Inventory / stats screen |
| `J` | Journal (lore entries) |
| `M` | Toggle enlarged minimap |
| `Esc` | Settings overlay (resume / FOV / volume) |
| `R` or `Enter` | Restart after death |

## Gameplay

### Waves

Wave definitions are parsed at startup from `assets/waves.txt`:

```
WAVE "Display Name" duration_sec
GROUP at_sec Name count [Name count ...]
```

`at_sec` is the group's spawn offset from wave start. A wave ends when its
duration elapses regardless of remaining enemies; the HUD shows the
current wave name and countdown.

Shipped wave list:

1. **First Contact: Calici** — Cats + Chickens (introduction).
2. **Avian Strain** — Heavier chicken pressure.
3. **Twisted** — Harpies join the mix.
4. **Hydrophobia** — Cat + Harpy sustained pressure.
5. **Parvoviral** — Bulldogs introduced.
6. **Distemper** — All four small types together.
7. **Hemorrhage** — Pigs added.
8. **Blue Tongue** — Full roster.
9. **Lockjaw** — High-density mixed swarms.
10. **Final Hour: Wasting** — 90 s endurance finale.

After wave 1 the **Tyranno** boss spawns 14 m in front of the player,
fronted by a 5.6 s roar cutscene. The boss bypasses the wave HP scaler
(240 HP verbatim), aggros from 40 m, and plays a dedicated `Attack`
one-shot whenever it lands a hit.

### Enemies

All five enemies are driven by an immutable `EnemyDef` table entry
(`src/game/EnemyDef.h`) carrying model, walk clip, scale, hit radius,
fix-up rotations, HP, speed, damage and attack cadence.

| Name | HP | Speed | Damage | Notes |
| --- | --- | --- | --- | --- |
| Cat | 2 | 3.2 m/s | 3 | Aggro `Cat_Run` clip at 9 m (×2 speed); plays `Cat_Death` then fades out. Hisses on hit. |
| Chicken | 4 | 2.7 m/s | 3.5 | No skeleton — procedural Y-bob + forward lean. |
| Harpy | 8 | 2.4 m/s | 6 | Flier; uses the GLB's `simple flyght` clip. |
| Pig | 14 | 2.0 m/s | 8 | Single bundled animation. |
| Bulldog | 20 | 1.7 m/s | 11 | Procedural 4-leg diagonal gait built at startup against the GLB skeleton. |
| **Tyranno (boss)** | 240 | 2.4 m/s | 28 | Roar / Run / Attack / Fall clips. Spawns after wave 1. |

Per-wave HP is rescaled (`kHpScaleRatio = 0.45` of the player's projected
damage budget, capped at ×3) so a quiet wave plus a heavy build doesn't
yield 200-HP cats.

### Items, rarity and chests

Killed enemies have a per-type chance to drop a chest (`dropChance` in
the table — 5–50 % for normal mobs, 100 % for the boss). Chests auto-open,
flicker through rarity colours (rolled at the documented 50 / 25 / 15 / 8 / 2 %
distribution), fade out, and pop a clickable loot toast that grants the
rolled item.

| Item | Rarity tier | Effect |
| --- | --- | --- |
| AUTO RING | Common | Auto-fires at the nearest enemy; stacks raise rate. |
| ORBIT RING | Common | Orbits a syringe (radius 1.6 m) that damages on touch (0.4 s per-target cooldown). |
| HAIL RING | Uncommon | Every 5 s rains 35 + 18/stack syringes within 10 m. |
| EXPLOSIVE AUTO | Rare | Auto-fire shots burst on impact (AoE 2.8 m, 3 dmg). |
| LIGHTNING RING | Rare | Auto-fire chains to the 3 nearest enemies (1 dmg each). |

Items are passive and permanent; duplicates stack additively.

### Progression

Killing enemies grants XP. Each level-up presents three picks from the
shared upgrade pool (`src/game/Upgrade.h`):

- **ATHLETE** — +15 % walking speed
- **ADRENALINE** — +20 % sprint speed
- **FAST TRIGGER** — +25 % fire rate
- **HYPER-NEEDLE** — +40 % projectile speed
- **TWIN NEEDLE** — +1 syringe per burst
- **SPREAD SHOT** — +1 syringe in the fan

Base stats start at 150 HP, 10 damage/syringe, 2 syringes/sec, 20 stamina,
4 m/s walk × 1.7 sprint multiplier, 7.5 m/s jump under 25 m/s² gravity.

### Antidote caches

Ten antidote boxes spawn around the map across a 60 s grace interval at
the start of the run; collecting all ten is the win condition advertised
in the briefing.

## Architecture

```
src/
├── main.cpp           game loop, scene state, draw orchestration,
│                      cutscene, wave HP scaling, item update loops
├── audio/Audio        miniaudio wrapper — engine + named sound bank,
│                      single-voice positional play
├── core/
│   ├── Window         GLFW + GLAD context, raw mouse, cursor lock
│   ├── Input          keyboard + mouse delta + edge tracking
│   └── Time           clamped dt
├── render/
│   ├── Shader         file-loaded GLSL, typed uniform setters
│   ├── Mesh           indexed VAO/VBO/EBO, Vertex {pos, normal, uv, joints, weights}
│   ├── Texture        RGBA bytes / stb_image / HDR
│   ├── Framebuffer    HDR colour + depth attachment for post-processing
│   ├── Model          OBJ via tinyobjloader, GLB via cgltf (skinning,
│   │                  per-vertex joint indices + weights, per-load tex cache)
│   ├── Animation      AnimationClip with translation/rotation/scale channels
│   ├── Animator       sampling, looping, one-shot with overlap, blending into rest pose
│   ├── Camera         FPS yaw/pitch, view + projection matrices
│   ├── PostFx         full-screen CRT pass (barrel, scanlines, aberration, grain, flicker)
│   ├── Hud            bar / rect / crosshair shaders
│   ├── Text           stb_easy_font text drawing
│   ├── Glow           additive billboard discs (chest auras, lantern halos)
│   ├── Particles      CPU point-sprite system
│   ├── Arrows         on-screen pointer arrows to off-screen objectives
│   ├── Minimap        compact/expanded disc with world-radius zoom
│   └── Skybox         equirectangular HDR background
└── game/
    ├── Player         FPS controller, walk-bob, view shake, trauma, knockback,
    │                  stamina, XP, level-up, inventory
    ├── Weapon / Projectile
    ├── Enemy / EnemyDef / EnemySpawner   wave-driven spawn ring, terrain-snap
    ├── Wave / WaveManager                parses waves.txt
    ├── Terrain        Perlin heightmap, bilinear height query, zone tinting
    ├── Chest / Item / Rng                rarity roll, auto-open, loot toast
    ├── AntidoteBox                       collectible objective
    ├── LevelUpMenu / StatsScreen / Journal / StartMenu / SettingsMenu
    ├── Settings                          horizontal FOV, master volume
    └── Scene                             top-level scene enum (StartMenu, Playing, Inventory, Journal, Settings)
```

### Boundaries

- `render/` knows nothing about `game/`. Game code submits data; the
  renderer never mutates game state.
- `EnemyDef`, `Item`, `WaveDef`, `Upgrade` are pure data; the systems
  that consume them live in `game/` or `main.cpp`.
- Resource classes (`Mesh`, `Texture`, `Model`, `Framebuffer`, `Glow`,
  `Particles`, `Skybox`) are move-only RAII; no raw GL handles outside
  `render/`.
- Shaders live in `assets/shaders/` and are loaded at runtime, so we can
  iterate without recompiling C++.

## Assets

```
assets/
├── models/    GLB enemies, props, syringe, treasure chest, antidote box,
│              dead trees, tombstones, bats, lantern, tyranno boss
├── shaders/   world, post_crt, hud_bar, hud_crosshair, text, glow_disc,
│              particle, sky, minimap_disc, arrow
├── textures/  ground / wall albedos + satara_night_1k.hdr skybox
├── audio/     ambient, footsteps, attacks, voice-over, pickup / level
│              cues, boss intro / roar / stomp
└── waves.txt  wave definitions (see Waves above)

data/
├── player_brief.txt   start-menu briefing copy
└── Journal.csv        in-game journal entries (lore)
```

## Conventions

- C++20, `/W4 /permissive-` on MSVC, `-Wall -Wextra -Wpedantic` on GCC.
- Tunables live near the top of `main()` or in their type's header
  (`kEnemyRadius`, `kChestOpenDuration`, `kOrbitRadius`, `kHailInterval`,
  etc.) — no magic numbers buried mid-function.
- No emojis in source or commits.
- Frequent, small commits as work progresses.

## Scope

What's in:

- Forward-shaded scene with one flashlight spotlight + tiny ambient
  (still pitch-black-ish — that's the horror).
- Procedural Perlin terrain as the level — no AABB walls or BSP geometry.
- GLB/PNG/HDR asset pipeline (no Blender export round-trips required).
- Windows-only `.exe` + `assets/` + `data/` folders.

What's intentionally out:

- Real per-light shading (multiple point lights, shadow maps). Chest
  auras and lantern halos are billboarded additive discs, not real lights.
- AABB level / wall geometry.
- Networking, savegames, controller input.
- New asset pipelines beyond GLB / PNG / HDR.

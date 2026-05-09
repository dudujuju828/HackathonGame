# HackathonGame — Claude working notes

A wave-based FPS horror in C++20 with a from-scratch OpenGL 3.3 renderer.
Originally a 3-day hackathon scope; the renderer and game systems have grown
past the initial cuts. Whole team uses Claude Code, so any AI attribution in
commits is fine — don't strip `Co-Authored-By` lines.

## Workflow

- **Commit frequently.** Small, incremental commits as work progresses,
  not one big dump at the end. After any self-contained change (new module,
  passing build, working feature), commit it.
- **Build before committing** when feasible. The exe being held open by a
  running game can block the linker on Windows — close the running instance
  (or `Stop-Process -Name HackathonGame -Force`) before rebuilding.
- **Auto-launch convention**: end-of-turn behaviour is to launch the exe
  after a successful build (see `memory/feedback_launch_exe.md`). Use
  `Start-Process -FilePath "$PWD\build\HackathonGame.exe" -WorkingDirectory "$PWD\build"`
  so the relative `assets/` path resolves.
- Push to `origin/main` when commits accumulate. Feature branches are used
  for larger work (chests, animal enemies, waves) and merged with `--no-ff`
  so the grouping stays visible in history.

## Scope

What's in:

- **Terrain**: Perlin heightmap (`game::Terrain`), 3 named maps (Hollow
  Creek / Ashwood / The Mire), zone tinting. Bilinear-sampled height query
  for placement.
- **Enemies**: 5 full-skeletal-animated GLB types (Harpy, Bulldog, Cat,
  Pig, Chicken). Each driven by an `EnemyDef` table entry that carries
  model, walk clip, scale, hit radius, HP, and per-model fix-up rotations
  (`basePitchDeg`, `baseYawDeg`, `baseRollDeg`) for GLBs with non-standard
  axis conventions.
- **Waves**: parsed at startup from `assets/waves.txt`. Each wave has a
  display name, a duration budget, and groups that fire at specific
  seconds-since-wave-start. HUD shows current wave + countdown.
- **Lighting**: flashlight spotlight + tiny ambient (still pitch-black-ish).
  Pure forward shading; no point lights yet (chest auras are billboarded
  additive discs, not real lights).
- **Items / chests**: enemies drop chests (50% rate). Chests auto-open,
  flicker through rarity colours (50/25/15/8/2), fade out, and pop a loot
  toast that grants the rolled item on click. Two items shipped:
  `AutoSyringeRing` (auto-fires) and `OrbitalRing` (orbits + collides).
  Items stack additively.
- **Player**: FPS controller with walk-bob, view shake, sprint-w-stamina,
  XP/level-up menu (3-pick upgrades), inventory screen on `B`.
- **Post-FX**: full-screen CRT shader (barrel, scanlines, aberration,
  grain, flicker).
- **Particles + glow**: CPU point-sprite system + additive ground discs
  used by the chest opening effect.

What's intentionally out (don't add without asking):

- Real per-light shading (multiple point lights, shadow maps).
- AABB level/wall geometry — terrain is the level.
- Networking, savegames, controller input.
- New asset pipelines beyond GLB/PNG.

## Architecture

```
src/
├── main.cpp           game loop, scene state, draw orchestration
├── core/              Window (GLFW + GLAD), Input, Time
├── render/            Shader, Mesh, Texture, Camera, Framebuffer,
│                      Model (OBJ + GLB via cgltf, with skinning),
│                      Animation + Animator (translation/rotation/scale
│                      channel interpolation, loop and one-shot),
│                      Hud (bars/rect/crosshair), Text (stb_easy_font),
│                      PostFx (CRT), Glow (additive disc), Particles (point sprites)
└── game/              Player, Weapon, Projectile,
                       Enemy + EnemyDef + EnemySpawner,
                       Wave + WaveManager, Terrain,
                       Chest + Item + Rng,
                       Scene + Settings + SettingsMenu,
                       LevelUpMenu + Upgrade + StatsScreen
```

Boundaries:

- `render/` knows nothing about `game/`. Game code submits data; renderer
  never mutates game state.
- `EnemyDef` / `Item` / `WaveDef` are pure data; the systems that consume
  them live in `game/` or `main.cpp`.

## Assets

```
assets/
├── models/    GLB enemies + chest + syringe (some Z-up, hence basePitchDeg)
├── shaders/   world, post_crt, hud_bar, hud_crosshair, text, glow_disc, particle
└── waves.txt  wave definitions, parsed at startup
```

`waves.txt` format:

```
WAVE "Display Name" duration_sec
GROUP at_sec  Name count [Name count ...]
```

- `at_sec` is relative to wave start. Wave ends after `duration_sec`
  regardless of remaining enemies.
- Names are case-insensitive and must match `EnemyDef::name`.

## Build

```sh
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --parallel
build/HackathonGame.exe   # assets/ is auto-copied next to the exe
```

GLAD2's generator needs Python `jinja2` on a fresh clone:
`python -m pip install jinja2`.

Dependencies are fetched via CMake `FetchContent` (GLFW 3.4, GLAD 2.0.6,
GLM 1.0.1, stb, miniaudio 0.11.21, tinyobjloader, cgltf 1.14). No system
package required.

## Conventions

- C++20, `/W4 /permissive-` on MSVC, `-Wall -Wextra -Wpedantic` on GCC.
- Resource classes (`Mesh`, `Texture`, `Model`, `Framebuffer`, `Glow`,
  `Particles`) are move-only RAII; no raw GL handles outside `render/`.
- Shaders live in `assets/shaders/` and are loaded at runtime so we can
  iterate without recompiling.
- All gameplay tunables live near the top of `main()` or in their type's
  header (`kEnemyRadius`, `kChestOpenDuration`, `kOrbitRadius`, etc.) —
  no magic numbers buried mid-function.
- No emojis in source or commits.

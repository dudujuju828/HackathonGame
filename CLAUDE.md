# HackathonGame — Claude working notes

3-day hackathon FPS horror in C++20 with a from-scratch OpenGL renderer.
Whole team uses Claude Code, so any AI attribution in commits is fine —
don't strip `Co-Authored-By` lines.

## Workflow

- **Commit frequently.** Small, incremental commits as work progresses,
  not one big dump at the end. After any self-contained change (new module,
  passing build, working feature), commit it.
- Build before committing when feasible. The exe being held open by a
  running game can block the linker on Windows — close the running instance
  before rebuilding.
- Push to `origin/main` when commits accumulate; no PRs for hackathon pace.

## Scope (locked)

- Levels: hand-coded AABB boxes in C++.
- Enemies: billboard sprites (Doom-style), no skeletal animation.
- Lighting: flashlight spotlight + tiny ambient. Pitch-black scene.
- Platform: Windows-only `.exe` + assets folder.

If a change pulls toward more (level format, animation system, shadow maps),
flag it and ask before doing it. 3 days means ruthless cuts.

## Architecture

```
src/
├── main.cpp
├── core/    Window, Input, Time
├── render/  Shader, Texture, Mesh, Model, Camera, (Renderer next), Sprite
├── physics/ Collision (AABB + swept-AABB)
├── audio/   Audio (miniaudio wrapper)
└── game/    Player, Weapon, Enemy, Level, GameState
```

Boundary: `render/` knows nothing about `game/`. Game code submits data;
renderer never mutates game state.

## Build

```sh
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --parallel
build/HackathonGame.exe   # assets/ is auto-copied next to the exe
```

GLAD2's generator needs Python `jinja2` on a fresh clone:
`python -m pip install jinja2`.

## Conventions

- C++20, `/W4 /permissive-` on MSVC, `-Wall -Wextra -Wpedantic` on GCC.
- Resource classes (`Mesh`, `Texture`, `Model`) are move-only RAII; no
  raw GL handles outside `render/`.
- Shaders live in `assets/shaders/` and are loaded at runtime so we can
  iterate without recompiling.
- No emojis in source or commits.

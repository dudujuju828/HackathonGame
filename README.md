# HackathonGame

A first-person horror shooter built from scratch in C++20 with a custom
OpenGL renderer. 3-day hackathon project.

## Status

Day 1 — engine foundation. **Not yet a game.** The current build opens a
window, locks the cursor, and lets you walk a checker-textured cube grid
with WASD + mouse-look. Use it to verify the renderer/input/camera work
end-to-end.

## Build

Requires: CMake ≥ 3.20, a C++20 compiler (tested with MinGW GCC 14.2),
Python 3 with `jinja2` (used by GLAD2's generator on first configure).

```sh
python -m pip install jinja2

cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --parallel

build/HackathonGame.exe
```

`assets/` is auto-copied next to the executable on every build, so
relative paths in code (`assets/shaders/world.vert`, etc.) work from
the build directory.

All third-party dependencies are pulled in automatically via CMake
`FetchContent` — no package manager, no submodules:

| Library | Version | Purpose |
| --- | --- | --- |
| GLFW | 3.4 | Window + input |
| GLAD2 | 2.0.6 | OpenGL 3.3 core loader |
| GLM | 1.0.1 | Math (vec/mat/quat) |
| stb_image | master | Texture decoding |
| tinyobjloader | 2.0.0rc13 | OBJ + MTL parsing |
| miniaudio | 0.11.21 | Audio (not yet wired up) |

## Controls

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` | Move |
| Mouse | Look |
| `Esc` | Quit |

## Architecture

```
src/
├── main.cpp              walkable smoke test (cube grid + floor)
├── core/
│   ├── Window            GLFW + GL 3.3 context, raw mouse, cursor lock
│   ├── Input             keys + mouse delta
│   └── Time              clamped dt
└── render/
    ├── Shader            file-loaded GLSL, typed uniform setters
    ├── Texture           RGBA bytes or stb_image file load
    ├── Mesh              indexed VAO/VBO/EBO, Vertex {pos, normal, uv}
    ├── Model             OBJ+MTL via tinyobjloader; material groups,
    │                     vertex dedup, per-load texture cache
    └── Camera            FPS yaw/pitch, view + projection matrices
```

Boundary kept clean: `render/` knows nothing about gameplay. Resource
classes (`Mesh`, `Texture`, `Model`) are move-only RAII; no GL handles
leak outside `render/`.

Shaders are loaded at runtime from `assets/shaders/` so they can be
edited and reloaded without rebuilding C++. `world.vert/frag` already
implements a flashlight spotlight + tiny ambient — the lighting model
the game will ship with.

## What's next

- `render/Renderer` — draw-command abstraction so game code submits
  data instead of calling GL directly.
- `physics/Collision` — AABB + swept-AABB for player movement.
- `audio/Audio` — miniaudio wrapper (ambient drone, gunshot, footsteps).
- `game/` — Player, Weapon, Enemy (billboard sprites), Level
  (hand-coded AABBs), GameState.

## Scope (locked)

- Hand-coded AABB level geometry — no level format, no Blender pipeline.
- Billboard sprite enemies — no skeletal animation.
- Flashlight spotlight + tiny ambient — no shadow maps.
- Windows-only `.exe` + `assets/` folder.

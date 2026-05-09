#include "core/Input.h"
#include "core/Time.h"
#include "core/Window.h"
#include "game/Chest.h"
#include "game/Enemy.h"
#include "game/Item.h"
#include "game/Rng.h"
#include "game/Terrain.h"
#include "game/EnemySpawner.h"
#include "game/LevelUpMenu.h"
#include "game/Player.h"
#include "game/Projectile.h"
#include "game/Scene.h"
#include "game/SettingsMenu.h"
#include "game/StatsScreen.h"
#include "game/Upgrade.h"
#include "game/Wave.h"
#include "game/WaveManager.h"
#include "game/Weapon.h"
#include "render/Camera.h"
#include "render/Framebuffer.h"
#include "render/Glow.h"
#include "render/Hud.h"
#include "render/Mesh.h"
#include "render/Model.h"
#include "render/Particles.h"
#include "render/PostFx.h"
#include "render/Shader.h"
#include "render/Text.h"
#include "render/Texture.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

namespace {

// Procedural walk AnimationClip for Bulldog (38-joint skeleton).
// Joint indices come from the Bulldog.glb skeleton dump:
//   0  _rootJoint     (root)
//   2  spine01_02     (mid-spine)
//  14  frontLeg01.L / 19 frontLeg01.R
//  24  hindLeg01.L  / 31 hindLeg01.R
render::AnimationClip buildDogWalk(const render::Skeleton& skel) {
    render::AnimationClip clip;
    clip.name     = "ProceduralWalk";
    clip.duration = 0.7f;  // one full stride ~1.4 strides/s

    constexpr float kPi = 3.14159265358979f;
    const float     T   = clip.duration;

    // Swing a joint ±amplitude around its local X axis (forward/backward for leg bones).
    // phaseOffset = pi flips the leg to opposite position → diagonal gait.
    auto addSwing = [&](int jointIdx, float amplitude, float phaseOffset) {
        if (jointIdx >= static_cast<int>(skel.joints.size())) return;
        render::AnimationChannel ch;
        ch.targetJoint = jointIdx;
        const glm::quat rest = skel.joints[jointIdx].rotation;
        for (int k = 0; k <= 8; ++k) {
            float t     = T * (static_cast<float>(k) / 8.0f);
            float angle = amplitude * std::sin(phaseOffset + (2.0f * kPi / T) * t);
            ch.rotations.push_back({ t, rest * glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f)) });
        }
        clip.channels.push_back(std::move(ch));
    };

    // Diagonal gait: front-L & hind-R swing together; front-R & hind-L swing together.
    addSwing(14, glm::radians(30.0f), 0.0f);   // frontLeg01.L
    addSwing(19, glm::radians(30.0f), kPi);    // frontLeg01.R
    addSwing(24, glm::radians(35.0f), kPi);    // hindLeg01.L  (anti-phase with front-L)
    addSwing(31, glm::radians(35.0f), 0.0f);   // hindLeg01.R  (in-phase with front-L)

    // Spine lateral sway at 2x stride frequency.
    {
        render::AnimationChannel ch;
        ch.targetJoint = 2;  // spine01_02
        const glm::quat rest = skel.joints[2].rotation;
        for (int k = 0; k <= 8; ++k) {
            float t   = T * (static_cast<float>(k) / 8.0f);
            float ang = glm::radians(3.0f) * std::sin((4.0f * kPi / T) * t);
            ch.rotations.push_back({ t, rest * glm::angleAxis(ang, glm::vec3(0.0f, 0.0f, 1.0f)) });
        }
        clip.channels.push_back(std::move(ch));
    }

    // Root vertical bounce at 2x stride frequency (rises on each footfall).
    {
        render::AnimationChannel ch;
        ch.targetJoint = 0;  // _rootJoint
        const glm::vec3 restT = skel.joints[0].translation;
        for (int k = 0; k <= 8; ++k) {
            float t = T * (static_cast<float>(k) / 8.0f);
            float y = 0.025f * std::abs(std::sin((2.0f * kPi / T) * t));
            ch.translations.push_back({ t, restT + glm::vec3(0.0f, y, 0.0f) });
        }
        clip.channels.push_back(std::move(ch));
    }

    return clip;
}

void buildCube(std::vector<render::Vertex>& verts, std::vector<uint32_t>& idx) {
    struct Face { glm::vec3 n; glm::vec3 a, b, c, d; };
    const Face faces[6] = {
        // +X
        { {1,0,0},  {0.5f,-0.5f, 0.5f}, {0.5f,-0.5f,-0.5f}, {0.5f, 0.5f,-0.5f}, {0.5f, 0.5f, 0.5f} },
        // -X
        {{-1,0,0}, {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f} },
        // +Y
        { {0,1,0},  {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f} },
        // -Y
        { {0,-1,0}, {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f} },
        // +Z
        { {0,0,1},  {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f} },
        // -Z
        { {0,0,-1}, { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f} },
    };
    for (const auto& f : faces) {
        uint32_t base = static_cast<uint32_t>(verts.size());
        verts.push_back({f.a, f.n, {0, 0}, glm::ivec4(0), glm::vec4(0.0f)});
        verts.push_back({f.b, f.n, {1, 0}, glm::ivec4(0), glm::vec4(0.0f)});
        verts.push_back({f.c, f.n, {1, 1}, glm::ivec4(0), glm::vec4(0.0f)});
        verts.push_back({f.d, f.n, {0, 1}, glm::ivec4(0), glm::vec4(0.0f)});
        idx.insert(idx.end(), {base+0, base+1, base+2, base+0, base+2, base+3});
    }
}

// 64x64 RGBA checker in two colors.
std::vector<uint8_t> makeChecker(int size, uint8_t r1, uint8_t g1, uint8_t b1,
                                 uint8_t r2, uint8_t g2, uint8_t b2, int cell = 8) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool on = ((x / cell) + (y / cell)) & 1;
            size_t i = (static_cast<size_t>(y) * size + x) * 4;
            px[i+0] = on ? r1 : r2;
            px[i+1] = on ? g1 : g2;
            px[i+2] = on ? b1 : b2;
            px[i+3] = 255;
        }
    }
    return px;
}

}  // namespace

int main() {
    try {
        core::Window window(1280, 720, "HackathonGame");
        core::Input  input;
        core::Time   time;
        input.attach(window.handle());

        render::Shader worldShader;
        if (!worldShader.loadFromFiles("assets/shaders/world.vert",
                                       "assets/shaders/world.frag")) {
            return 1;
        }

        render::Mesh cube;
        {
            std::vector<render::Vertex> v;
            std::vector<uint32_t>       i;
            buildCube(v, i);
            cube.upload(v, i);
        }

        render::Texture checker;
        auto px = makeChecker(64, 220, 220, 220, 60, 60, 60);
        checker.createRGBA(64, 64, px.data(), /*nearest=*/true);

        game::Terrain terrain;
        terrain.generate();

        game::Player player;
        player.setSpawn({ 0.0f, terrain.heightAt(0.0f, 3.0f), 3.0f });

        render::Model syringeModel;
        if (!syringeModel.loadFromFile("assets/models/syringe.glb")) {
            std::fprintf(stderr, "[main] failed to load syringe model\n");
        }

        // Enemy models — one per type, loaded once and shared across all instances.
        render::Model harpyModel, bulldogModel, catModel, pigModel, chickenModel;
        auto loadEnemy = [](render::Model& m, const char* path) {
            if (!m.loadFromFile(path))
                std::fprintf(stderr, "[main] failed to load %s\n", path);
        };
        loadEnemy(harpyModel,   "assets/models/Harpy.glb");
        loadEnemy(bulldogModel, "assets/models/Bulldog.glb");
        loadEnemy(catModel,     "assets/models/Cat.glb");
        loadEnemy(pigModel,     "assets/models/Pig.glb");
        loadEnemy(chickenModel, "assets/models/Chicken.glb");

        // Build the procedural walk clip for Bulldog now that the skeleton is loaded.
        render::AnimationClip bulldogWalkClip;
        if (bulldogModel.skeleton())
            bulldogWalkClip = buildDogWalk(bulldogModel.skeleton().value());

        // Find a named animation clip; returns nullptr if the model has none or name not found.
        auto findAnim = [](const render::Model& m, const char* name) -> const render::AnimationClip* {
            for (const auto& clip : m.animations())
                if (clip.name == name) return &clip;
            return m.animations().empty() ? nullptr : &m.animations()[0];
        };

        // Per-type tuning.
        // Harpy   -> "simple flyght"       real skeletal flight anim
        // Bulldog -> bulldogWalkClip        procedural skeletal walk (4-leg diagonal gait)
        // Cat     -> no clips in GLB        procedural Y-bob walk substitute
        // Pig     -> "ArmatureAction"       only clip in GLB
        // Chicken -> no skeleton/clips      procedural bob + forward lean in render matrix
        //
        // name (for waves.txt lookup),  model, walkAnim, scale, height, radius, bobFreq, bobAmp, basePitchDeg, baseYawDeg, baseRollDeg, maxHp
        const game::EnemyDef enemyDefs[] = {
            { "Harpy",   &harpyModel,   findAnim(harpyModel, "simple flyght"), 0.30f, 1.70f, 0.60f, 0.0f,  0.0f,    0.0f,   0.0f,   0.0f,   3 },
            { "Bulldog", &bulldogModel, &bulldogWalkClip,                       1.50f, 0.70f, 0.60f, 0.0f,  0.0f,  -90.0f,   0.0f,   0.0f,   5 },
            { "Cat",     &catModel,     nullptr,                                0.45f, 0.25f, 0.40f, 8.0f,  0.04f,   0.0f,   0.0f,   0.0f,   1 },
            { "Pig",     &pigModel,     findAnim(pigModel, "ArmatureAction"),   0.60f, 0.35f, 0.50f, 0.0f,  0.0f,    0.0f,   0.0f,   0.0f,   4 },
            { "Chicken", &chickenModel, nullptr,                                2.50f, 0.80f, 0.50f, 16.0f, 0.06f, -90.0f, -90.0f,  20.0f,   2 },
        };

        render::Model chestModel;
        if (!chestModel.loadFromFile("assets/models/treasure_chest.glb")) {
            std::fprintf(stderr, "[main] failed to load chest model\n");
        }
        const render::AnimationClip* chestOpenAnim = nullptr;
        if (!chestModel.animations().empty()) {
            chestOpenAnim = &chestModel.animations()[0];
        }

        game::Weapon weapon;
        weapon.setModel(&syringeModel);
        weapon.unlimited = true;
        weapon.autoFire  = true;
        weapon.fireRate  = 1.0f / player.attackSpeed;
        weapon.viewmodel.scale = 0.035f;

        std::vector<game::Projectile> projectiles;
        projectiles.reserve(64);
        float projectileSpeed = 25.0f;  // m/s
        const float projectileScale = 0.05f;  // match the held viewmodel
        const float projectileShrink = 0.75f; // 0..1, fraction of scale lost by maxAge

        std::vector<game::Enemy> enemies;
        enemies.reserve(32);
        game::EnemySpawner spawner;
        spawner.intervalSec    = 2.0f;
        spawner.spawnMinRadius = 14.0f;  // never closer than this to the player
        spawner.spawnRadius    = 22.0f;  // upper bound of the random spawn distance
        spawner.setDefs(enemyDefs, static_cast<int>(std::size(enemyDefs)));

        game::WaveManager waveManager;
        {
            std::vector<game::WaveDef> waves;
            if (game::loadWaves("assets/waves.txt", waves)) {
                waveManager.init(std::move(waves), &spawner);
            } else {
                std::fprintf(stderr, "[main] no waves loaded — using fallback random spawner\n");
            }
        }

        std::vector<game::Chest> chests;
        chests.reserve(16);
        float ringCooldown = 0.0f;

        std::vector<game::ItemInstance> pendingLoot;
        bool prevLootClick = false;

        // Orbital syringes: state for OrbitalRing item. cooldowns_[i] gates how
        // often syringe i can damage. Resized lazily as the player gains rings.
        constexpr float kOrbitRadius      = 1.6f;   // metres around the player
        constexpr float kOrbitSpeedRad    = 2.5f;   // rad/s
        constexpr float kOrbitYOffset     = 1.0f;   // above player feet
        constexpr float kOrbitalScale     = 0.05f;  // syringe model scale
        constexpr float kOrbitalHitRadius = 0.45f;  // collision sphere
        constexpr float kOrbitalCooldown  = 0.4f;   // seconds before a single syringe can hit again
        float orbitalAngle = 0.0f;
        std::vector<float> orbitalCooldowns;

        render::Framebuffer sceneFbo;
        sceneFbo.resize(window.width(), window.height());

        render::PostFx postFx;
        if (!postFx.init("assets/shaders/post_crt.vert",
                         "assets/shaders/post_crt.frag")) {
            return 1;
        }

        render::Hud hud;
        if (!hud.init("assets/shaders")) {
            return 1;
        }
        render::Text text;
        if (!text.init("assets/shaders/text.vert",
                       "assets/shaders/text.frag")) {
            return 1;
        }

        render::Glow glow;
        if (!glow.init("assets/shaders/glow_disc.vert",
                       "assets/shaders/glow_disc.frag")) {
            return 1;
        }
        render::Particles particles;
        if (!particles.init("assets/shaders/particle.vert",
                            "assets/shaders/particle.frag")) {
            return 1;
        }

        game::SettingsMenu settingsMenu;
        game::LevelUpMenu  levelUpMenu;
        game::StatsScreen  statsScreen;
        game::Scene scene = game::Scene::Playing;
        bool prevEscape   = false;
        bool prevB        = false;
        int  prevMapIndex = settingsMenu.settings().mapIndex;

        glClearColor(0.02f, 0.02f, 0.03f, 1.0f);

        while (!window.shouldClose()) {
            window.pollEvents();
            input.update();
            time.tick();
            float dt = time.dt();

            // Escape edge-toggles the settings overlay; it no longer quits.
            const bool curEscape = input.key(GLFW_KEY_ESCAPE);
            if (curEscape && !prevEscape) {
                scene = (scene == game::Scene::Playing)
                          ? game::Scene::Settings
                          : game::Scene::Playing;
                glfwSetInputMode(window.handle(), GLFW_CURSOR,
                                 scene == game::Scene::Settings
                                   ? GLFW_CURSOR_NORMAL
                                   : GLFW_CURSOR_DISABLED);
                input.resetMouseDelta();
            }
            prevEscape = curEscape;

            // B toggles the stats/inventory screen while playing.
            const bool curB = input.key(GLFW_KEY_B);
            if (curB && !prevB) {
                if (scene == game::Scene::Playing) {
                    scene = game::Scene::Inventory;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    input.resetMouseDelta();
                } else if (scene == game::Scene::Inventory) {
                    scene = game::Scene::Playing;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    input.resetMouseDelta();
                }
            }
            prevB = curB;

            // Apply settings to the player camera every frame. The slider value
            // is horizontal FOV; convert to vertical FOV for glm::perspective.
            {
                const float aspect = window.aspect();
                const float hFovRad =
                    glm::radians(settingsMenu.settings().hFovDeg);
                const float vFovRad =
                    2.0f * std::atan(std::tan(hFovRad * 0.5f) / std::max(aspect, 0.001f));
                player.camera().fovDeg = glm::degrees(vFovRad);
            }

            if (scene == game::Scene::Playing) {
                // Trigger level-up menu if pending.
                if (player.pendingLevelUps() > 0) {
                    scene = game::Scene::LevelUp;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    input.resetMouseDelta();
                    levelUpMenu.reset();
                } else {
                    float gh = terrain.heightAt(player.position().x, player.position().z);
                    player.update(dt, input, time.total(), gh);
                    weapon.fireRate = 1.0f / player.attackSpeed;
                    weapon.update(dt, input, player);
                }
            } else if (scene == game::Scene::Settings) {
                game::MenuAction action =
                    settingsMenu.update(dt, input,
                                        window.width(), window.height());
                if (action == game::MenuAction::ExitGame) break;
            } else if (scene == game::Scene::LevelUp) {
                auto upgrade = levelUpMenu.update(dt, input, window.width(), window.height());
                if (upgrade) {
                    // Apply upgrade.
                    switch (upgrade->type) {
                        case game::UpgradeType::MoveSpeed:
                            player.feel().moveSpeed *= upgrade->magnitude;
                            break;
                        case game::UpgradeType::SprintSpeed:
                            player.feel().sprintMultiplier *= upgrade->magnitude;
                            break;
                        case game::UpgradeType::FireRate:
                            weapon.fireRate *= upgrade->magnitude;
                            break;
                        case game::UpgradeType::ProjectileSpeed:
                            projectileSpeed *= upgrade->magnitude;
                            break;
                        case game::UpgradeType::ExtraProjectile:
                            weapon.projectileCount += (int)upgrade->magnitude;
                            break;
                        case game::UpgradeType::FanOut:
                            weapon.fanColumns += (int)upgrade->magnitude;
                            break;
                        default: break;
                    }
                    player.consumeLevelUp();
                    if (player.pendingLevelUps() == 0) {
                        scene = game::Scene::Playing;
                        glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        input.resetMouseDelta();
                    } else {
                        levelUpMenu.reset(); // Pick next set of options for subsequent level
                    }
                }
            }

            // Detect map switch from the settings menu and regenerate terrain.
            {
                const int newMap = settingsMenu.settings().mapIndex;
                if (newMap != prevMapIndex) {
                    prevMapIndex = newMap;
                    terrain.generate(newMap);
                    player.setSpawn({ 0.0f, terrain.heightAt(0.0f, 3.0f), 3.0f });
                    projectiles.clear();
                    enemies.clear();
                    chests.clear();
                    waveManager.reset();
                }
            }

            // Apply fullscreen toggle each frame (cheap if already in state).
            window.setFullscreen(settingsMenu.settings().fullscreen);

            const render::Camera& cam = player.camera();

            // Projectile spawn / advance only run while playing; the menu pauses
            // the world.
            if (scene == game::Scene::Playing) {
            // Spawn a projectile on the fire frame.
            if (weapon.firedThisFrame()) {
                glm::vec3 baseFwd   = cam.forward();
                glm::vec3 baseRight = glm::normalize(glm::cross(baseFwd, glm::vec3(0,1,0)));
                glm::vec3 up        = glm::cross(baseRight, baseFwd);

                const int cols = weapon.fanColumns;
                const float startAngle = (cols > 1) ? -weapon.fanSpreadDeg * 0.5f : 0.0f;
                const float stepAngle  = (cols > 1) ? weapon.fanSpreadDeg / (cols - 1) : 0.0f;

                for (int i = 0; i < cols; ++i) {
                    float deg = startAngle + i * stepAngle;
                    float rad = glm::radians(deg);

                    // Rotate the forward vector around the 'up' axis by 'rad'
                    glm::vec3 dir = glm::normalize(
                        baseFwd * std::cos(rad) + baseRight * std::sin(rad)
                    );

                    // Calculate a local right vector for this specific projectile's offset
                    glm::vec3 localRight = glm::normalize(glm::cross(dir, up));

                    // Use deterministic parallel positional offsets based on the burst index.
                    // This creates a precise "twin-blade" pattern that never diverges over distance.
                    int burstIndex = weapon.projectileCount - weapon.burstRemaining() - 1;
                    
                    // Center the pattern based on the total burst count.
                    float centeredIndex = static_cast<float>(burstIndex) - (weapon.projectileCount - 1) * 0.5f;
                    
                    // Multiply by a small spacing value (6 centimeters) to create a tight side-by-side blade
                    float offsetX = centeredIndex * 0.06f;
                    float offsetY = 0.0f;

                    game::Projectile p;
                    // Spawn slightly in front and to the right so it visually leaves
                    // the syringe tip rather than the camera centre.
                    p.position = cam.position + dir * 0.45f + localRight * (0.18f + offsetX) - up * (0.15f + offsetY);
                    p.velocity = dir * projectileSpeed;
                    p.scale    = projectileScale;
                    p.maxAge   = 3.0f;
                    projectiles.push_back(p);
                }
            }

            // Advance and cull projectiles.
            for (auto& p : projectiles) {
                p.position += p.velocity * dt;
                p.age      += dt;
            }
            // Spawn + advance enemies (also gated by Playing scene).
            // WaveManager owns scheduling now; falls back to the legacy spawner
            // tick only if no waves were loaded.
            const size_t enemiesBeforeSpawn = enemies.size();
            if (waveManager.totalWaves() > 0) {
                waveManager.update(dt, enemies, player.position());
            } else {
                spawner.update(dt, enemies, player.position());
            }
            // Snap freshly-spawned enemies to terrain height once. After this
            // their Y stays put — pathfinding is XZ only, so they don't bob.
            for (size_t i = enemiesBeforeSpawn; i < enemies.size(); ++i) {
                enemies[i].position.y = terrain.heightAt(enemies[i].position.x,
                                                          enemies[i].position.z);
            }

            for (auto& e : enemies) {
                if (e.alive() && e.def) {
                    e.update(dt, player.position());
                    e.animator.update(dt);
                    if (e.def->model->skeleton()) {
                        e.animator.calculateBoneTransforms(&e.def->model->skeleton().value());
                    }
                }
            }

            // Auto-fire ring: cooldown driven by stacked ring count. Every
            // (1 / N) seconds, spawn a syringe aimed at the nearest live enemy.
            {
                const float ringRate = player.autoRingRate();
                if (ringRate > 0.0f) {
                    ringCooldown -= dt;
                    if (ringCooldown <= 0.0f) {
                        // Find nearest enemy on XZ within reasonable range.
                        const float kAutoRange = 25.0f;
                        game::Enemy* target = nullptr;
                        float bestD2 = kAutoRange * kAutoRange;
                        for (auto& e : enemies) {
                            if (!e.alive()) continue;
                            glm::vec3 d = e.hitCentre() - cam.position;
                            float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                            if (d2 < bestD2) { bestD2 = d2; target = &e; }
                        }
                        if (target) {
                            // Spawn near the held-syringe tip but pushed further
                            // down so a behind-the-camera target doesn't cause
                            // the shot to fly past the player's face on the way out.
                            glm::vec3 baseFwd   = cam.forward();
                            glm::vec3 baseRight = glm::normalize(glm::cross(baseFwd, glm::vec3(0,1,0)));
                            glm::vec3 up        = glm::cross(baseRight, baseFwd);
                            glm::vec3 spawn = cam.position
                                            + baseFwd  * 0.45f
                                            + baseRight * 0.18f
                                            - up        * 0.45f;
                            glm::vec3 dir = glm::normalize(target->hitCentre() - spawn);

                            game::Projectile pj;
                            pj.position = spawn;
                            pj.velocity = dir * projectileSpeed;
                            pj.scale    = projectileScale;
                            pj.maxAge   = 3.0f;
                            projectiles.push_back(pj);
                            ringCooldown = 1.0f / ringRate;
                        } else {
                            // No target — re-check soon rather than burning a full second.
                            ringCooldown = 0.25f;
                        }
                    }
                } else {
                    ringCooldown = 0.0f;
                }
            }

            // Orbital syringes: deterministic positions around the player based
            // on stacked OrbitalRings. Each syringe carries its own damage
            // cooldown so a single stationary enemy can't be infinite-ticked.
            {
                const int orbitalCount = player.countItem(game::ItemId::OrbitalRing);
                if (static_cast<int>(orbitalCooldowns.size()) != orbitalCount) {
                    orbitalCooldowns.assign(orbitalCount, 0.0f);
                }
                if (orbitalCount > 0) {
                    orbitalAngle += kOrbitSpeedRad * dt;
                    if (orbitalAngle > 6.28318530718f) orbitalAngle -= 6.28318530718f;

                    const glm::vec3 centre = player.position()
                                           + glm::vec3(0.0f, kOrbitYOffset, 0.0f);
                    const float step = 6.28318530718f / static_cast<float>(orbitalCount);
                    for (int i = 0; i < orbitalCount; ++i) {
                        if (orbitalCooldowns[i] > 0.0f) orbitalCooldowns[i] -= dt;
                        const float a = orbitalAngle + step * static_cast<float>(i);
                        const glm::vec3 pos = centre + glm::vec3(std::cos(a) * kOrbitRadius,
                                                                 0.0f,
                                                                 std::sin(a) * kOrbitRadius);
                        if (orbitalCooldowns[i] > 0.0f) continue;
                        for (auto& e : enemies) {
                            if (!e.alive()) continue;
                            if (glm::distance(pos, e.hitCentre()) <
                                game::kEnemyRadius + kOrbitalHitRadius) {
                                e.hp -= 1;
                                orbitalCooldowns[i] = kOrbitalCooldown;
                                if (!e.alive()) {
                                    player.addXp(game::kEnemyXpReward);
                                    if (game::rand01() < game::kChestDropChance) {
                                        game::Chest c;
                                        c.position   = e.position;
                                        c.yawRad     = game::rand01() * 6.28318530718f;
                                        c.rolled     = game::rollChestRarity();
                                        c.itemId     = game::rollChestItem();
                                        c.state      = game::ChestState::Opening;
                                        c.openTimer  = 0.0f;
                                        c.cyclePhase = 1.0f;
                                        c.animator.setAnimation(chestOpenAnim, /*loop=*/false);
                                        chests.push_back(c);
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }

            // Projectile <-> enemy hit detection. Sphere-vs-point. First alive
            // enemy within radius takes the hit and ends the projectile.
            for (auto& p : projectiles) {
                if (!p.alive()) continue;
                for (auto& e : enemies) {
                    if (!e.alive()) continue;
                    float hitRadius = e.def ? e.def->radius : game::kEnemyRadius;
                    if (glm::distance(p.position, e.hitCentre()) < hitRadius) {
                        e.hp -= 1;
                        p.age = p.maxAge;          // mark projectile for cull
                        if (!e.alive()) {
                            player.addXp(game::kEnemyXpReward);
                            // Roll for chest drop.
                            if (game::rand01() < game::kChestDropChance) {
                                game::Chest c;
                                c.position   = e.position;
                                c.yawRad     = game::rand01() * 6.28318530718f;
                                c.rolled     = game::rollChestRarity();
                                c.itemId     = game::rollChestItem();
                                c.state      = game::ChestState::Opening;
                                c.openTimer  = 0.0f;
                                c.cyclePhase = 1.0f;  // forces a tint pick on the first tick
                                c.animator.setAnimation(chestOpenAnim, /*loop=*/false);
                                chests.push_back(c);
                            }
                        }
                        break;
                    }
                }
            }

            // Advance opening chests; flicker the tint and trigger the loot
            // popup when the cycle finishes. Item is granted on click, not here.
            for (auto& c : chests) {
                if (c.state == game::ChestState::Opening) {
                    c.openTimer += dt;
                    c.animator.update(dt);
                    if (chestModel.skeleton()) {
                        c.animator.calculateBoneTransforms(&chestModel.skeleton().value());
                    }
                    // Random colour swap rate eases from 12 Hz → 3 Hz.
                    const float u = std::clamp(c.openTimer / game::kChestOpenDuration,
                                               0.0f, 1.0f);
                    const float rate = glm::mix(12.0f, 3.0f, u);
                    c.cyclePhase += rate * dt;
                    while (c.cyclePhase >= 1.0f) {
                        c.cyclePhase -= 1.0f;
                        std::uniform_int_distribution<int> pick(0, game::kRarityCount - 1);
                        c.cycleTint = game::rarityColor(
                            static_cast<game::Rarity>(pick(game::rng())));
                    }
                    if (c.openTimer >= game::kChestOpenDuration) {
                        c.state     = game::ChestState::Fading;
                        c.fadeTimer = 0.0f;
                        c.cycleTint = game::rarityColor(c.rolled);
                        pendingLoot.push_back({ c.itemId, c.rolled });
                    }

                    // Emit upward-rising particles in the chest's current colour.
                    const float emitInterval = 1.0f / 45.0f;
                    c.emitTimer += dt;
                    while (c.emitTimer >= emitInterval) {
                        c.emitTimer -= emitInterval;
                        const float angle = game::rand01() * 6.28318530718f;
                        const float r     = game::rand01() * 0.18f;
                        const float gh    = terrain.heightAt(c.position.x, c.position.z);
                        render::Particle pp;
                        pp.position = glm::vec3(c.position.x + std::cos(angle) * r,
                                                gh + 0.05f,
                                                c.position.z + std::sin(angle) * r);
                        pp.velocity = glm::vec3((game::rand01() - 0.5f) * 0.6f,
                                                1.4f + game::rand01() * 0.9f,
                                                (game::rand01() - 0.5f) * 0.6f);
                        pp.color    = glm::vec4(c.currentTint(), 1.0f);
                        pp.life     = 0.9f;
                        pp.age      = 0.0f;
                        pp.size     = 22.0f;
                        particles.emit(pp);
                    }
                }
            }

            // Cull dead enemies.
            enemies.erase(
                std::remove_if(enemies.begin(), enemies.end(),
                               [](const game::Enemy& e){ return !e.alive(); }),
                enemies.end());

            // Re-cull projectiles that died from a hit.
            projectiles.erase(
                std::remove_if(projectiles.begin(), projectiles.end(),
                               [](const game::Projectile& p){ return !p.alive(); }),
                projectiles.end());
            }  // end Playing-scene gate

            // Particle simulation runs regardless of scene so existing puffs
            // continue to drift while the loot popup is up.
            particles.update(dt);

            // Chest fade ticks regardless of scene so the visual completes
            // while the loot popup is up. Keep the animator alive too so the
            // skinned pose holds at the final open frame.
            for (auto& c : chests) {
                if (c.state == game::ChestState::Fading) {
                    c.fadeTimer += dt;
                    c.animator.update(dt);
                    if (chestModel.skeleton()) {
                        c.animator.calculateBoneTransforms(&chestModel.skeleton().value());
                    }
                    if (c.fadeTimer >= game::kChestFadeDuration) {
                        c.state = game::ChestState::Done;
                    }
                }
            }
            chests.erase(
                std::remove_if(chests.begin(), chests.end(),
                    [](const game::Chest& c){ return c.state == game::ChestState::Done; }),
                chests.end());

            // Hand off to the loot popup as soon as a chest finishes opening.
            if (!pendingLoot.empty() && scene == game::Scene::Playing) {
                scene = game::Scene::Loot;
                glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                input.resetMouseDelta();
                prevLootClick = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
            }

            // Loot scene: click anywhere to claim the front item and continue.
            if (scene == game::Scene::Loot) {
                const bool curClick = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
                const bool justClicked = curClick && !prevLootClick;
                prevLootClick = curClick;
                if (justClicked && !pendingLoot.empty()) {
                    player.grantItem(pendingLoot.front());
                    pendingLoot.erase(pendingLoot.begin());
                    if (pendingLoot.empty()) {
                        scene = game::Scene::Playing;
                        glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        input.resetMouseDelta();
                    }
                }
            }

            // Keep the scene FBO matched to the window size.
            sceneFbo.resize(window.width(), window.height());
            sceneFbo.bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            worldShader.use();
            checker.bind(0);
            worldShader.setInt  ("uAlbedo", 0);
            worldShader.setInt  ("uHasBones", 0);
            worldShader.setFloat("uAlpha",   1.0f);
            worldShader.setMat4 ("uViewProj", cam.proj(window.aspect()) * cam.view());
            worldShader.setVec3 ("uCamPos", cam.position);
            worldShader.setVec3 ("uCamDir", cam.forward());
            worldShader.setVec3 ("uAmbient", glm::vec3(0.04f));
            {
                const float outerDeg = settingsMenu.settings().flashlightDeg;
                const float innerDeg = outerDeg * 0.6f;  // hot core ~60% of outer
                worldShader.setFloat("uFlashInner",
                                     glm::cos(glm::radians(innerDeg)));
                worldShader.setFloat("uFlashOuter",
                                     glm::cos(glm::radians(outerDeg)));
            }
            worldShader.setVec3 ("uFlashColor", glm::vec3(1.2f, 1.15f, 1.0f));
            worldShader.setVec3 ("uTint", glm::vec3(1.0f));  // default for world geometry

            // Terrain mesh (replaces the old flat floor + cube grid).
            terrain.texture().bind(0);
            worldShader.setMat4("uModel", glm::mat4(1.0f));
            terrain.mesh().draw();
            checker.bind(0); // restore for subsequent geometry

            // Enemies: track the active def to avoid redundant shader state changes
            // when consecutive enemies share a type.
            if (!enemies.empty()) {
                worldShader.setVec3("uTint", glm::vec3(1.0f));
                const game::EnemyDef* activeDef = nullptr;

                for (const auto& e : enemies) {
                    if (!e.def) continue;

                    if (e.def != activeDef) {
                        activeDef = e.def;
                        worldShader.setInt("uHasBones",
                            activeDef->model->skeleton().has_value() ? 1 : 0);
                    }

                    // Procedural walk bob for animals with no skeletal animation.
                    float bob = 0.0f;
                    if (activeDef->bobFreq > 0.0f && glm::length(e.velocity) > 1e-4f) {
                        float phase = e.position.x * 0.31f + e.position.z * 0.71f;
                        bob = std::sin(time.total() * activeDef->bobFreq + phase)
                              * activeDef->bobAmp;
                    }

                    glm::mat4 M(1.0f);
                    M = glm::translate(M, e.position + glm::vec3(0.0f, activeDef->height + bob, 0.0f));
                    const bool moving = glm::length(e.velocity) > 1e-4f;
                    if (moving) {
                        glm::vec3 fwd = glm::normalize(e.velocity);
                        float angle = std::atan2(fwd.x, fwd.z);
                        M = glm::rotate(M, angle, glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                    // baseRoll is applied right after the chase yaw so it acts
                    // as a true side-lean around the model's final forward axis,
                    // regardless of which way the enemy is facing.
                    if (activeDef->baseRollDeg != 0.0f) {
                        M = glm::rotate(M, glm::radians(activeDef->baseRollDeg),
                                        glm::vec3(0.0f, 0.0f, 1.0f));
                    }
                    // Per-model fix-up rotations. baseYaw is applied first so it
                    // turns the model after basePitch puts it upright; together
                    // they reorient an arbitrary GLB into the engine's
                    // "Y up, +Z forward" convention.
                    if (activeDef->baseYawDeg != 0.0f) {
                        M = glm::rotate(M, glm::radians(activeDef->baseYawDeg),
                                        glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                    if (activeDef->basePitchDeg != 0.0f) {
                        M = glm::rotate(M, glm::radians(activeDef->basePitchDeg),
                                        glm::vec3(1.0f, 0.0f, 0.0f));
                    }
                    // Chicken has no skeleton: simulate running posture with a forward lean
                    // and a slight side-to-side roll that syncs with the vertical bob.
                    if (activeDef->bobFreq > 0.0f && moving) {
                        float phase = e.position.x * 0.31f + e.position.z * 0.71f;
                        float roll  = glm::radians(6.0f)
                                      * std::sin(time.total() * activeDef->bobFreq * 0.5f + phase);
                        M = glm::rotate(M, glm::radians(-20.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // lean forward
                        M = glm::rotate(M, roll,                  glm::vec3(0.0f, 0.0f, 1.0f)); // side sway
                    }
                    M = glm::scale(M, glm::vec3(activeDef->scale));
                    worldShader.setMat4("uModel", M);

                    if (activeDef->model->skeleton()) {
                        const auto& matrices = e.animator.finalBoneMatrices();
                        for (size_t i = 0; i < matrices.size() && i < 100; ++i) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "uBones[%zu]", i);
                            worldShader.setMat4(buf, matrices[i]);
                        }
                    }

                    for (const auto& sub : activeDef->model->meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        sub.mesh.draw();
                    }
                }
                worldShader.setInt("uHasBones", 0);
            }

            // Chests on the ground.
            if (!chests.empty()) {
                bool chestHasBones = chestModel.skeleton().has_value();
                GLboolean blendWas = glIsEnabled(GL_BLEND);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                for (const auto& c : chests) {
                    glm::mat4 M(1.0f);
                    float gh = terrain.heightAt(c.position.x, c.position.z);
                    M = glm::translate(M, glm::vec3(c.position.x,
                                                    gh + game::kChestYOffset,
                                                    c.position.z));
                    M = glm::rotate(M, c.yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
                    M = glm::rotate(M, glm::radians(game::kChestBasePitchDeg),
                                    glm::vec3(1.0f, 0.0f, 0.0f));
                    M = glm::scale(M, glm::vec3(game::kChestScale));
                    worldShader.setMat4 ("uModel", M);
                    worldShader.setVec3 ("uTint",  c.currentTint());
                    worldShader.setFloat("uAlpha", c.currentAlpha());

                    const bool useBones = chestHasBones &&
                                          c.state != game::ChestState::Closed;
                    worldShader.setInt("uHasBones", useBones ? 1 : 0);
                    if (useBones) {
                        const auto& matrices = c.animator.finalBoneMatrices();
                        for (size_t i = 0; i < matrices.size() && i < 100; ++i) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "uBones[%zu]", i);
                            worldShader.setMat4(buf, matrices[i]);
                        }
                    }

                    for (const auto& sub : chestModel.meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        sub.mesh.draw();
                    }
                }
                worldShader.setInt  ("uHasBones", 0);
                worldShader.setVec3 ("uTint",     glm::vec3(1.0f));
                worldShader.setFloat("uAlpha",    1.0f);
                if (!blendWas) glDisable(GL_BLEND);
                checker.bind(0);

                // Additive ground discs under each active chest.
                const glm::mat4 vp = cam.proj(window.aspect()) * cam.view();
                for (const auto& c : chests) {
                    if (c.state == game::ChestState::Closed) continue;
                    const float gh = terrain.heightAt(c.position.x, c.position.z);
                    const glm::vec3 centre(c.position.x, gh + 0.02f, c.position.z);
                    const float intensity = (c.state == game::ChestState::Fading)
                        ? 1.4f * c.currentAlpha()
                        : 1.4f;
                    glow.draw(vp, centre, /*radius=*/2.5f,
                              c.currentTint(), intensity);
                }
                // World shader was active before — re-bind for any subsequent draws.
                worldShader.use();
            }

            // Particles render after chests so they sit on top of the glow disc.
            // Drawn unconditionally so puffs outlive their parent chest.
            {
                const glm::mat4 vp = cam.proj(window.aspect()) * cam.view();
                particles.draw(vp, window.height());
                worldShader.use();
                checker.bind(0);
            }

            // In-flight projectiles (depth-tested against the world).
            if (!projectiles.empty()) {
                GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
                glDisable(GL_CULL_FACE);
                for (const auto& p : projectiles) {
                    glm::vec3 vfwd = glm::length(p.velocity) > 1e-4f
                                       ? glm::normalize(p.velocity)
                                       : glm::vec3(0, 0, -1);
                    glm::vec3 ref  = std::abs(vfwd.y) > 0.99f
                                       ? glm::vec3(1, 0, 0)
                                       : glm::vec3(0, 1, 0);
                    glm::vec3 vrt  = glm::normalize(glm::cross(vfwd, ref));
                    glm::vec3 vup  = glm::cross(vrt, vfwd);

                    glm::mat4 M(1.0f);
                    M[0] = glm::vec4(vrt,         0.0f);
                    M[1] = glm::vec4(vup,         0.0f);
                    M[2] = glm::vec4(-vfwd,       0.0f);
                    M[3] = glm::vec4(p.position,  1.0f);
                    // Shrink with age to exaggerate distance / speed.
                    float ageFrac = p.maxAge > 0.0f ? p.age / p.maxAge : 0.0f;
                    float shrinkMul = 1.0f - projectileShrink * ageFrac;
                    M = glm::scale(M, glm::vec3(p.scale * shrinkMul));

                    worldShader.setMat4("uModel", M);
                    for (const auto& sub : syringeModel.meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        sub.mesh.draw();
                    }
                }
                if (cullWas) glEnable(GL_CULL_FACE);
            }

            // Orbital syringes: drawn at deterministic positions around the
            // player. Same syringe model as the in-flight projectiles.
            {
                const int orbitalCount = player.countItem(game::ItemId::OrbitalRing);
                if (orbitalCount > 0) {
                    GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
                    glDisable(GL_CULL_FACE);
                    const glm::vec3 centre = player.position()
                                           + glm::vec3(0.0f, kOrbitYOffset, 0.0f);
                    const float step = 6.28318530718f / static_cast<float>(orbitalCount);
                    for (int i = 0; i < orbitalCount; ++i) {
                        const float a = orbitalAngle + step * static_cast<float>(i);
                        const glm::vec3 pos = centre + glm::vec3(std::cos(a) * kOrbitRadius,
                                                                 0.0f,
                                                                 std::sin(a) * kOrbitRadius);
                        // Point the syringe tip radially outward from the player.
                        const glm::vec3 fwd(std::cos(a), 0.0f, std::sin(a));
                        const glm::vec3 up(0.0f, 1.0f, 0.0f);
                        const glm::vec3 right = glm::normalize(glm::cross(fwd, up));
                        const glm::vec3 vup   = glm::cross(right, fwd);

                        glm::mat4 M(1.0f);
                        M[0] = glm::vec4(right, 0.0f);
                        M[1] = glm::vec4(vup,   0.0f);
                        M[2] = glm::vec4(-fwd,  0.0f);
                        M[3] = glm::vec4(pos,   1.0f);
                        M = glm::scale(M, glm::vec3(kOrbitalScale));

                        worldShader.setMat4("uModel", M);
                        for (const auto& sub : syringeModel.meshes()) {
                            if (sub.diffuse) sub.diffuse->bind(0);
                            sub.mesh.draw();
                        }
                    }
                    if (cullWas) glEnable(GL_CULL_FACE);
                }
            }

            // Held viewmodel: clear depth so the syringe never clips into walls.
            glClear(GL_DEPTH_BUFFER_BIT);
            weapon.draw(worldShader, cam);

            // Post-process pass to default framebuffer.
            render::Framebuffer::bindDefault(window.width(), window.height());
            glClear(GL_COLOR_BUFFER_BIT);
            postFx.apply(sceneFbo.colorTexture(),
                         window.width(), window.height(),
                         time.total(),
                         postFx.params());

            if (scene == game::Scene::Playing) {
                hud.xpBar().level    = player.level();
                hud.xpBar().xp       = player.xp();
                hud.xpBar().xpToNext = player.xpToNext();

                // Health bar driven by actual player stats.
                hud.healthBar().fraction =
                    (player.maxHealth > 0.0f)
                    ? std::clamp(player.health / player.maxHealth, 0.0f, 1.0f)
                    : 0.0f;

                hud.drawXpBar     (window.width(), window.height(), hud.xpBar());
                hud.drawHealthBar (window.width(), window.height(), time.total(),
                                   hud.healthBar());

                // Stamina bar — sits directly above the health bar, same left margin.
                {
                    const float margin  = hud.healthBar().marginPx.x;
                    const float hpH     = hud.healthBar().sizePx.y;
                    const float hpBot   = hud.healthBar().marginPx.y;  // from bottom
                    const float stH     = 8.0f;
                    const float gap     = 5.0f;
                    const float stW     = hud.healthBar().sizePx.x;
                    const float originY = static_cast<float>(window.height())
                                         - hpBot - hpH - gap - stH;
                    const float stFrac  = (player.maxStamina > 0.0f)
                        ? std::clamp(player.stamina / player.maxStamina, 0.0f, 1.0f)
                        : 0.0f;
                    hud.drawProgress(window.width(), window.height(),
                                     glm::vec2(margin, originY),
                                     glm::vec2(stW, stH),
                                     stFrac,
                                     glm::vec3(0.20f, 0.75f, 0.85f),
                                     glm::vec3(0.02f, 0.06f, 0.08f),
                                     glm::vec3(0.15f, 0.45f, 0.55f),
                                     1.0f, 0.92f);
                }

                hud.drawCrosshair (window.width(), window.height(), hud.crosshair());

                // "LV N" label centred above the XP bar.
                char lvlBuf[16];
                std::snprintf(lvlBuf, sizeof(lvlBuf), "LV %d", player.level());
                const std::string lvl = lvlBuf;
                const float lvlScale  = 1.5f;
                const float lvlW      = render::Text::measure(lvl) * lvlScale;
                text.draw(window.width(), window.height(),
                          window.width() * 0.5f - lvlW * 0.5f,
                          hud.xpBar().topPx - 14.0f,
                          lvl, lvlScale,
                          glm::vec4(0.85f, 1.0f, 0.85f, 1.0f));

                // Wave name + countdown, top-centre below the XP bar.
                if (waveManager.totalWaves() > 0) {
                    char waveBuf[96];
                    if (waveManager.finished()) {
                        std::snprintf(waveBuf, sizeof(waveBuf), "ALL WAVES CLEAR");
                    } else {
                        std::snprintf(waveBuf, sizeof(waveBuf), "WAVE %d / %d  %s  %.0fs",
                                      waveManager.currentWaveIndex() + 1,
                                      waveManager.totalWaves(),
                                      waveManager.currentWaveName().c_str(),
                                      waveManager.waveTimeRemaining());
                    }
                    const std::string ws = waveBuf;
                    const float wScale = 1.6f;
                    const float wW     = render::Text::measure(ws) * wScale;
                    text.draw(window.width(), window.height(),
                              window.width() * 0.5f - wW * 0.5f,
                              hud.xpBar().topPx + hud.xpBar().sizePx.y + 14.0f,
                              ws, wScale,
                              glm::vec4(1.0f, 0.92f, 0.78f, 1.0f));
                }
            } else if (scene == game::Scene::Settings) {
                settingsMenu.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::LevelUp) {
                levelUpMenu.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::Loot) {
                if (!pendingLoot.empty()) {
                    const auto& item = pendingLoot.front();
                    const int W = window.width();
                    const int H = window.height();

                    const float panelW = 460.0f;
                    const float panelH = 200.0f;
                    const float panelX = W * 0.5f - panelW * 0.5f;
                    const float panelY = H * 0.5f - panelH * 0.5f;

                    const glm::vec3 rcol  = game::rarityColor(item.rarity);
                    const glm::vec3 panel { 0.05f, 0.04f, 0.04f };

                    hud.drawRect(W, H, glm::vec2(0, 0),
                                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                                 glm::vec3(0.0f), 0.55f);
                    hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                                     glm::vec2(panelW, panelH),
                                     1.0f, panel, panel, rcol, 2.5f, 0.96f);

                    {
                        const std::string title = "YOU OBTAINED";
                        const float sc = 1.8f;
                        const float w  = render::Text::measure(title) * sc;
                        text.draw(W, H, panelX + (panelW - w) * 0.5f,
                                  panelY + 28.0f, title, sc,
                                  glm::vec4(0.85f, 0.80f, 0.78f, 1.0f));
                    }
                    {
                        const std::string rar = game::rarityName(item.rarity);
                        const float sc = 1.6f;
                        const float w  = render::Text::measure(rar) * sc;
                        text.draw(W, H, panelX + (panelW - w) * 0.5f,
                                  panelY + 60.0f, rar, sc, glm::vec4(rcol, 1.0f));
                    }
                    {
                        const std::string nm = game::itemName(item.id);
                        const float sc = 3.5f;
                        const float w  = render::Text::measure(nm) * sc;
                        text.draw(W, H, panelX + (panelW - w) * 0.5f,
                                  panelY + 92.0f, nm, sc, glm::vec4(rcol, 1.0f));
                    }
                    {
                        const std::string hint = "[CLICK TO CONTINUE]";
                        const float sc = 1.4f;
                        const float w  = render::Text::measure(hint) * sc;
                        text.draw(W, H, panelX + (panelW - w) * 0.5f,
                                  panelY + panelH - 32.0f, hint, sc,
                                  glm::vec4(0.65f, 0.55f, 0.55f, 1.0f));
                    }
                }
            } else if (scene == game::Scene::Inventory) {
                game::PlayerStats ps;
                ps.health       = player.health;
                ps.maxHealth    = player.maxHealth;
                ps.damage       = player.damage;
                ps.attackSpeed  = player.attackSpeed;
                ps.stamina      = player.stamina;
                ps.maxStamina   = player.maxStamina;
                ps.level        = player.level();
                ps.autoRingRate = player.autoRingRate();
                ps.orbitalCount = player.countItem(game::ItemId::OrbitalRing);
                ps.items        = player.inventory();
                statsScreen.draw(hud, text, window.width(), window.height(), ps);
            }

            window.swap();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}



#include "core/Input.h"
#include "core/Time.h"
#include "core/Window.h"
#include "game/Enemy.h"
#include "game/Terrain.h"
#include "game/EnemySpawner.h"
#include "game/LevelUpMenu.h"
#include "game/Player.h"
#include "game/Projectile.h"
#include "game/Scene.h"
#include "game/SettingsMenu.h"
#include "game/StatsScreen.h"
#include "game/Upgrade.h"
#include "game/Weapon.h"
#include "render/Camera.h"
#include "render/Framebuffer.h"
#include "render/Hud.h"
#include "render/Mesh.h"
#include "render/Model.h"
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
#include <vector>

namespace {

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

        render::Model enemyModel;
        if (!enemyModel.loadFromFile("assets/models/Harpy.glb")) {
            std::fprintf(stderr, "[main] failed to load enemy model\n");
        }
        if(enemyModel.skeleton()){printf("Harpy has skeleton with %zu joints\n", enemyModel.skeleton()->joints.size());}else{printf("Harpy has NO skeleton!\n");}
const render::AnimationClip* runAnim = nullptr;
        if (!enemyModel.animations().empty()) {
            runAnim = &enemyModel.animations()[0];
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
        spawner.intervalSec = 4.0f;
        spawner.spawnRadius = 14.0f;

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

            // Keep enemies on the terrain surface.
            for (auto& e : enemies) {
                if (e.alive())
                    e.position.y = terrain.heightAt(e.position.x, e.position.z);
            }

            // Advance and cull projectiles.
            for (auto& p : projectiles) {
                p.position += p.velocity * dt;
                p.age      += dt;
            }
            // Spawn + advance enemies (also gated by Playing scene).
            size_t oldSize = enemies.size();
            spawner.update(dt, enemies, player.position());
            for (size_t i = oldSize; i < enemies.size(); ++i) {
                enemies[i].animator.setAnimation(runAnim);
            }

            for (auto& e : enemies) {
                if (e.alive()) {
                    e.update(dt, player.position());
                    e.animator.update(dt);
                    if (enemyModel.skeleton()) {
                        e.animator.calculateBoneTransforms(&enemyModel.skeleton().value());
                    }
                }
            }

            // Projectile <-> enemy hit detection. Sphere-vs-point. First alive
            // enemy within radius takes the hit and ends the projectile.
            for (auto& p : projectiles) {
                if (!p.alive()) continue;
                for (auto& e : enemies) {
                    if (!e.alive()) continue;
                    if (glm::distance(p.position, e.hitCentre()) < game::kEnemyRadius) {
                        e.hp -= 1;
                        p.age = p.maxAge;          // mark projectile for cull
                        if (!e.alive()) {
                            player.addXp(game::kEnemyXpReward);
                        }
                        break;
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

            // Keep the scene FBO matched to the window size.
            sceneFbo.resize(window.width(), window.height());
            sceneFbo.bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            worldShader.use();
            checker.bind(0);
            worldShader.setInt  ("uAlbedo", 0);
            worldShader.setInt  ("uHasBones", 0);
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

            // Enemies: animated Harpy models.
            if (!enemies.empty()) {
                worldShader.setVec3("uTint", glm::vec3(1.0f));
                bool hasBones = enemyModel.skeleton().has_value();
                worldShader.setInt("uHasBones", hasBones ? 1 : 0);

                for (const auto& e : enemies) {
                    glm::mat4 M(1.0f);
                    // The Harpy model origin is centered. Lift it so its feet touch the floor.
                    M = glm::translate(M, e.position + glm::vec3(0.0f, game::kEnemyHeight, 0.0f));
                    
                    // Rotate to face velocity
                    if (glm::length(e.velocity) > 1e-4f) {
                        glm::vec3 fwd = glm::normalize(e.velocity);
                        float angle = std::atan2(fwd.x, fwd.z);
                        M = glm::rotate(M, angle, glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                    
                    M = glm::scale(M, glm::vec3(0.3f)); 
                    worldShader.setMat4("uModel", M);

                    if (hasBones) {
                        const auto& matrices = e.animator.finalBoneMatrices();
                        for (size_t i = 0; i < matrices.size() && i < 100; ++i) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "uBones[%zu]", i);
                            worldShader.setMat4(buf, matrices[i]);
                        }
                    }

                    for (const auto& sub : enemyModel.meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        sub.mesh.draw();
                    }
                }
                worldShader.setInt("uHasBones", 0);
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
            } else if (scene == game::Scene::Settings) {
                settingsMenu.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::LevelUp) {
                levelUpMenu.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::Inventory) {
                game::PlayerStats ps;
                ps.health      = player.health;
                ps.maxHealth   = player.maxHealth;
                ps.damage      = player.damage;
                ps.attackSpeed = player.attackSpeed;
                ps.stamina     = player.stamina;
                ps.maxStamina  = player.maxStamina;
                ps.level       = player.level();
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



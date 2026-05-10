#include "audio/Audio.h"
#include "core/Input.h"
#include "core/Time.h"
#include "core/Window.h"
#include "game/AntidoteBox.h"
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
#include "game/Journal.h"
#include "game/StartMenu.h"
#include "game/StatsScreen.h"
#include "game/Upgrade.h"
#include "game/Wave.h"
#include "game/WaveManager.h"
#include "game/Weapon.h"
#include "render/Arrows.h"
#include "render/Camera.h"
#include "render/Framebuffer.h"
#include "render/Glow.h"
#include "render/Hud.h"
#include "render/Mesh.h"
#include "render/Minimap.h"
#include "render/Model.h"
#include "render/Particles.h"
#include "render/PostFx.h"
#include "render/Shader.h"
#include "render/Skybox.h"
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

// Procedural walk for the Cat (28-joint skeleton, no shipped animation).
// Joint indices come from the Cat.glb skeleton dump:
//   0   _rootJoint
//   2   shoulder hub (children: spine.3, front legs 6 & 10)
//   6   front leg L (hip)   / 10 front leg R (hip)
//  20   hind leg L (hip)    / 24 hind leg R (hip)
//  14   tail base (longest chain: 14..19)
render::AnimationClip buildCatWalk(const render::Skeleton& skel) {
    render::AnimationClip clip;
    clip.name     = "ProceduralWalk";
    clip.duration = 0.95f;  // slower stride — feels less frantic

    constexpr float kPi = 3.14159265358979f;
    const float     T   = clip.duration;

    // Swing a joint around its local X axis around the rest pose.
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

    // Diagonal gait: front-L & hind-R together, front-R & hind-L together.
    addSwing( 6, glm::radians(28.0f), 0.0f);   // front leg
    addSwing(10, glm::radians(28.0f), kPi);    // front leg (opposite)
    addSwing(20, glm::radians(32.0f), kPi);    // hind  leg (anti-phase with 6)
    addSwing(24, glm::radians(32.0f), 0.0f);   // hind  leg (in-phase with 6)

    // Tail base swishes laterally (Z) at stride frequency.
    {
        render::AnimationChannel ch;
        ch.targetJoint = 14;
        if (14 < static_cast<int>(skel.joints.size())) {
            const glm::quat rest = skel.joints[14].rotation;
            for (int k = 0; k <= 8; ++k) {
                float t   = T * (static_cast<float>(k) / 8.0f);
                float ang = glm::radians(15.0f) * std::sin((2.0f * kPi / T) * t);
                ch.rotations.push_back({ t, rest * glm::angleAxis(ang, glm::vec3(0.0f, 0.0f, 1.0f)) });
            }
            clip.channels.push_back(std::move(ch));
        }
    }

    // Spine hub sways subtly side-to-side.
    {
        render::AnimationChannel ch;
        ch.targetJoint = 2;
        if (2 < static_cast<int>(skel.joints.size())) {
            const glm::quat rest = skel.joints[2].rotation;
            for (int k = 0; k <= 8; ++k) {
                float t   = T * (static_cast<float>(k) / 8.0f);
                float ang = glm::radians(4.0f) * std::sin((4.0f * kPi / T) * t);
                ch.rotations.push_back({ t, rest * glm::angleAxis(ang, glm::vec3(0.0f, 0.0f, 1.0f)) });
            }
            clip.channels.push_back(std::move(ch));
        }
    }

    // Root vertical bounce at 2x stride frequency.
    {
        render::AnimationChannel ch;
        ch.targetJoint = 0;
        const glm::vec3 restT = skel.joints[0].translation;
        for (int k = 0; k <= 8; ++k) {
            float t = T * (static_cast<float>(k) / 8.0f);
            float y = 0.04f * std::abs(std::sin((2.0f * kPi / T) * t));
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
        core::Window window(2133, 1200, "HackathonGame");
        core::Input  input;
        core::Time   time;
        input.attach(window.handle());

        audio::Audio audio;
        if (!audio.init()) {
            std::fprintf(stderr, "[main] audio init failed — continuing silent\n");
        }
        audio.loadSound("footsteps", "assets/audio/footsteps_crunch.mp3", /*looping=*/true);
        audio.loadSound("ambient",   "assets/audio/ambient_static.mp3",   /*looping=*/true);
        audio.loadSound("ambient2",  "assets/audio/ambient_layer.mp3",    /*looping=*/true);
        // Bias the static a touch higher than the source recording so the
        // crackle reads as signal interference rather than rumble.
        audio.setPitch ("ambient",  1.25f);
        audio.setVolume("ambient",  0.55f);
        // Second layer sits underneath the static at neutral pitch so the
        // two textures interact rather than fight.
        audio.setPitch ("ambient2", 1.0f);
        audio.setVolume("ambient2", 0.45f);

        // Footstep loop: smooth a 0..1 envelope toward 1 while moving on the
        // ground. The clip is ~6 s of continuous crunching; we don't restart
        // it per-step — we just gate gain by motion and bend pitch a little
        // when sprinting so the cadence inside the clip keeps up.
        float footstepGain = 0.0f;

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

        render::Texture wallTex;
        if (!wallTex.loadFromFile("assets/textures/plastered_stone_wall_diff_1k.jpg",
                                   /*nearest=*/false)) {
            std::fprintf(stderr, "[main] wall texture missing — walls will use the checker fallback\n");
        }

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

        // Cat now ships with real animation clips (Cat_Walk, Cat_Run, etc.) —
        // procedural fallback no longer needed.

        // Find an animation clip by substring match (handles GLBs that
        // export with long Maya/Unreal-style prefixed names like
        // "SKM_Cat|SKM_Cat|Cat_Walk"). Returns nullptr only if the model
        // has no animations at all; otherwise falls back to the first clip.
        auto findAnim = [](const render::Model& m, const char* needle) -> const render::AnimationClip* {
            for (const auto& clip : m.animations())
                if (clip.name.find(needle) != std::string::npos) return &clip;
            return m.animations().empty() ? nullptr : &m.animations()[0];
        };

        // Per-type tuning.
        // Harpy   -> "simple flyght"       real skeletal flight anim
        // Bulldog -> bulldogWalkClip        procedural skeletal walk (4-leg diagonal gait)
        // Cat     -> no clips in GLB        procedural Y-bob walk substitute
        // Pig     -> "ArmatureAction"       only clip in GLB
        // Chicken -> no skeleton/clips      procedural bob + forward lean in render matrix
        //
        // name, model, walkAnim, scale, height, radius, bobFreq, bobAmp, basePitchDeg, baseYawDeg, baseRollDeg, maxHp, dropChance, moveSpeed, damage, attackRange, attackInterval
        const game::EnemyDef enemyDefs[] = {
            { "Harpy",   &harpyModel,   findAnim(harpyModel, "simple flyght"), 0.30f, 1.70f, 1.10f, 0.0f,  0.0f,    0.0f,   0.0f,   0.0f,   8,  0.20f,  2.4f,  12.0f, 1.7f, 0.9f },
            { "Bulldog", &bulldogModel, &bulldogWalkClip,                       1.50f, 0.70f, 1.10f, 0.0f,  0.0f,  -90.0f,   0.0f,   0.0f,  20,  0.50f,  1.7f,  22.0f, 1.8f, 1.3f },
            { "Cat",     &catModel,     findAnim(catModel, "Cat_Walk"),         0.027f, 0.25f, 1.30f, 0.0f,  0.0f,  -90.0f,   0.0f,   0.0f,   2,  0.05f,  3.2f,   6.0f, 1.5f, 0.45f,
              /*aggroAnim=*/findAnim(catModel, "Cat_Run"), /*aggroRange=*/9.0f, /*aggroSpeedMult=*/2.0f,
              /*deathAnim=*/findAnim(catModel, "Cat_Death") },
            { "Pig",     &pigModel,     findAnim(pigModel, "ArmatureAction"),   0.60f, 0.35f, 1.00f, 0.0f,  0.0f,    0.0f,   0.0f,   0.0f,  14,  0.30f,  2.0f,  16.0f, 1.7f, 1.1f },
            { "Chicken", &chickenModel, nullptr,                                4.00f, 0.20f, 1.00f, 16.0f, 0.06f, -90.0f, -90.0f,  20.0f,   4,  0.10f,  2.7f,   7.0f, 1.6f, 0.55f },
        };

        render::Model chestModel;
        if (!chestModel.loadFromFile("assets/models/treasure_chest.glb")) {
            std::fprintf(stderr, "[main] failed to load chest model\n");
        }

        render::Model antidoteBoxModel;
        if (!antidoteBoxModel.loadFromFile("assets/models/Antidote_box.glb")) {
            std::fprintf(stderr, "[main] failed to load antidote box model\n");
        }

        render::Model treeModel, deadTreeModel, lowPolyDeadTreeModel, tombstoneModel, batModel, lanternModel;
        if (!treeModel.loadFromFile("assets/models/spooky_tree_1.glb")) {
            std::fprintf(stderr, "[main] failed to load tree model\n");
        }
        if (!deadTreeModel.loadFromFile("assets/models/dead_tree.glb")) {
            std::fprintf(stderr, "[main] failed to load dead tree model\n");
        }
        if (!lowPolyDeadTreeModel.loadFromFile("assets/models/low_poly_dead_tree.glb")) {
            std::fprintf(stderr, "[main] failed to load low-poly dead tree model\n");
        }
        if (!tombstoneModel.loadFromFile("assets/models/stylized_tombstones.glb")) {
            std::fprintf(stderr, "[main] failed to load tombstone model\n");
        }
        if (!batModel.loadFromFile("assets/models/bat.glb")) {
            std::fprintf(stderr, "[main] failed to load bat model\n");
        }
        if (!lanternModel.loadFromFile("assets/models/old_lantern.glb")) {
            std::fprintf(stderr, "[main] failed to load lantern model\n");
        }

        // Atmospheric bats — tight ominous loops over the Graveyard quadrant
        // plus a few lazy big-radius wanderers across the rest of the sky.
        // Each bat owns its own Animator so flap animations play out of sync,
        // making the swarm look organic.
        struct Bat {
            glm::vec2 centerXZ;
            float     orbitRadius;
            float     orbitSpeed;   // rad/s; negative = counter-clockwise
            float     altitude;     // metres above world Y=0
            float     bobAmp;
            float     bobFreq;      // Hz
            float     scale;
            float     anglePhase;   // initial rotation offset
            float     animOffset;   // seconds; staggers each bat's flap cycle
            render::Animator animator;
        };
        std::vector<Bat> bats;
        const render::AnimationClip* batFlap = nullptr;
        if (!batModel.animations().empty()) {
            batFlap = &batModel.animations()[0];
        }
        {
            std::mt19937 rng(0xBEEFFEEDu);
            std::uniform_real_distribution<float> u01(0.0f, 1.0f);

            // 4 graveyard bats — tight, fast, low-ish loops over the SE
            // tombstone area.
            const glm::vec2 graveCenters[4] = {
                {  18.0f, -22.0f },
                {  38.0f, -16.0f },
                {  28.0f, -42.0f },
                {  46.0f, -38.0f },
            };
            for (int i = 0; i < 4; ++i) {
                Bat b;
                b.centerXZ    = graveCenters[i];
                b.orbitRadius = 4.0f + u01(rng) * 5.0f;        // 4-9 m
                b.orbitSpeed  = (u01(rng) < 0.5f ? -1.0f : 1.0f)
                              * (0.9f + u01(rng) * 0.7f);       // 0.9-1.6 rad/s
                b.altitude    = 7.0f + u01(rng) * 4.0f;         // 7-11 m
                b.bobAmp      = 0.5f + u01(rng) * 0.6f;
                b.bobFreq     = 1.0f + u01(rng) * 1.5f;
                b.scale       = (0.45f + u01(rng) * 0.25f) / 5.0f;
                b.anglePhase  = u01(rng) * 6.28318530718f;
                b.animOffset  = u01(rng);
                b.animator.setAnimation(batFlap, /*loop=*/true);
                bats.push_back(std::move(b));
            }

            // 4 wanderer bats — bigger circles across the rest of the map.
            const glm::vec2 wanderCenters[4] = {
                { -30.0f,  20.0f },
                {  10.0f,  35.0f },
                { -45.0f, -10.0f },
                {  -5.0f, -10.0f },
            };
            for (int i = 0; i < 4; ++i) {
                Bat b;
                b.centerXZ    = wanderCenters[i];
                b.orbitRadius = 12.0f + u01(rng) * 8.0f;        // 12-20 m
                b.orbitSpeed  = (u01(rng) < 0.5f ? -1.0f : 1.0f)
                              * (0.4f + u01(rng) * 0.4f);       // 0.4-0.8 rad/s
                b.altitude    = 12.0f + u01(rng) * 6.0f;        // 12-18 m
                b.bobAmp      = 0.8f + u01(rng) * 0.7f;
                b.bobFreq     = 0.6f + u01(rng) * 1.0f;
                b.scale       = (0.55f + u01(rng) * 0.30f) / 5.0f;
                b.anglePhase  = u01(rng) * 6.28318530718f;
                b.animOffset  = u01(rng);
                b.animator.setAnimation(batFlap, /*loop=*/true);
                bats.push_back(std::move(b));
            }

            // Stagger each bat's animation start so the swarm doesn't flap
            // in lockstep.
            for (auto& b : bats) {
                b.animator.update(b.animOffset);
            }
        }

        // Map decorations: deterministic procedural scatter so the layout
        // is the same every run. Trees cover the whole map; tombstones
        // are biased into the SE quadrant (Graveyard biome).
        struct Decoration {
            glm::vec3      position;
            float          yawRad;
            float          scale;
            float          basePitchDeg = 0.0f;  // X-axis fix-up (e.g. -90 for Z-up GLBs)
            float          baseYawDeg   = 0.0f;  // additional Y-axis fix-up applied after pitch
            render::Model* model;
        };
        std::vector<Decoration> decorations;
        {
            std::mt19937 rng(0xCAFEC0DEu);  // fixed seed = stable layout
            std::uniform_real_distribution<float> u01(0.0f, 1.0f);
            const float half = game::Terrain::kWorldSize * 0.5f;

            auto safeFromSpawn = [](float x, float z) {
                // Keep a 6m clear bubble around the player spawn (0, 3).
                const float dx = x - 0.0f;
                const float dz = z - 3.0f;
                return (dx * dx + dz * dz) > 36.0f;
            };

            // ~40 trees scattered everywhere within the playable area.
            for (int i = 0; i < 40; ++i) {
                const float x = (u01(rng) * 2.0f - 1.0f) * (half - 6.0f);
                const float z = (u01(rng) * 2.0f - 1.0f) * (half - 6.0f);
                if (!safeFromSpawn(x, z)) { --i; continue; }
                Decoration d;
                const float treeScale = (0.9f + u01(rng) * 0.7f) / 12.5f;
                d.position = glm::vec3(x, terrain.heightAt(x, z) + 2.8f, z);
                d.yawRad   = u01(rng) * 6.28318530718f;
                d.scale    = treeScale;
                d.model    = &treeModel;
                decorations.push_back(d);
            }

            // ~50 dead trees randomly scattered across the whole map; flips
            // a coin between the two variants to mix silhouettes.
            for (int i = 0; i < 50; ++i) {
                const float x = (u01(rng) * 2.0f - 1.0f) * (half - 6.0f);
                const float z = (u01(rng) * 2.0f - 1.0f) * (half - 6.0f);
                if (!safeFromSpawn(x, z)) { --i; continue; }
                Decoration d;
                const bool useLowPoly = u01(rng) < 0.5f;
                d.model = useLowPoly ? &lowPolyDeadTreeModel : &deadTreeModel;
                // Two models have different native sizes — pick scales by eye.
                d.scale = useLowPoly
                    ? (0.13f + u01(rng) * 0.10f)        // low_poly: small native (~3x smaller)
                    : (0.006f + u01(rng) * 0.004f);     // dead_tree: large native (~3x smaller)
                d.position = glm::vec3(x,
                                       terrain.heightAt(x, z) + (useLowPoly ? 0.0f : 0.5f),
                                       z);
                d.yawRad   = u01(rng) * 6.28318530718f;
                decorations.push_back(d);
            }

            // ~25 tombstones, biased into the SE Graveyard quadrant
            // (x in [0, half-6], z in [-(half-6), 0]).
            for (int i = 0; i < 25; ++i) {
                const float x = u01(rng) * (half - 6.0f);
                const float z = -u01(rng) * (half - 6.0f);
                if (!safeFromSpawn(x, z)) { --i; continue; }
                Decoration d;
                d.position = glm::vec3(x, terrain.heightAt(x, z), z);
                d.yawRad   = u01(rng) * 6.28318530718f;
                d.scale    = (0.6f + u01(rng) * 0.5f) / 25.0f;
                d.model    = &tombstoneModel;
                decorations.push_back(d);
            }

            // Old lanterns spaced evenly along all four perimeter walls,
            // inset slightly so they sit on our side of the wall.
            constexpr int   kLanternsPerSide = 7;
            constexpr float kLanternInset    = 2.5f;
            constexpr float kLanternScale    = 1.2f;
            const float perimSpan = (half - 6.0f);  // furthest lantern position along an axis

            auto addLantern = [&](float x, float z) {
                Decoration d;
                d.position     = glm::vec3(x, terrain.heightAt(x, z), z);
                d.yawRad       = u01(rng) * 6.28318530718f;
                d.scale        = kLanternScale;
                d.basePitchDeg = -90.0f;  // GLB ships Z-up; stand it upright
                d.baseYawDeg   = 180.0f;  // flip the lantern to face the way it should
                d.model        = &lanternModel;
                decorations.push_back(d);
            };

            for (int i = 0; i < kLanternsPerSide; ++i) {
                const float t       = -1.0f + 2.0f * static_cast<float>(i)
                                     / static_cast<float>(kLanternsPerSide - 1);
                const float varying = t * perimSpan;
                addLantern( varying,                  half - kLanternInset);  // north (+Z)
                addLantern( varying,                -(half - kLanternInset)); // south (-Z)
                addLantern( half - kLanternInset,    varying);                 // east  (+X)
                addLantern(-(half - kLanternInset),  varying);                 // west  (-X)
            }
        }

        // Perimeter walls: 4 long thin boxes around the map edge using the
        // existing unit cube mesh. Player position is clamped to keep them
        // inside the wall-bounded area (clamp inset is half the wall width).
        constexpr float kWallHalfThickness = 0.5f;     // metres (half-width)
        constexpr float kWallHalfHeight    = 8.0f;     // metres (above y=0)
        constexpr float kWallY             = 1.5f;     // wall centre Y (lowered)
        const float kWallHalfMap = game::Terrain::kWorldSize * 0.5f;
        const float kPlayerInset = 0.6f;               // a little less than wall thickness
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
        spawner.spawnMinRadius = 28.0f;  // ~2x previous so chase-mode enemies have room to ramp up
        spawner.spawnRadius    = 44.0f;  // upper bound of the random spawn distance
        spawner.setDefs(enemyDefs, static_cast<int>(std::size(enemyDefs)));

        // Difficulty scaling: per-wave HP multiplier on spawned enemies so
        // sumOfWaveHP >= kHpScaleRatio * (player DPS * wave duration).
        // Player DPS is approximated from current inventory composition.
        constexpr float kHpScaleRatio    = 1.2f;   // total wave HP target vs player damage budget
        constexpr float kHpScaleMaxMult  = 10.0f;  // cap so a tiny wave + heavy build doesn't make absurd 200-HP cats

        auto playerExpectedDps = [](const game::Player& p) -> float {
            float dps = 1.0f;  // baseline manual fire — ~1 dmg/sec assumed

            // Auto Ring: each stack = 1 shot/sec, 1 dmg per hit.
            dps += static_cast<float>(p.countItem(game::ItemId::AutoSyringeRing));

            // Orbit Ring: contact damage on a 0.4s per-syringe cooldown; assume
            // ~40% effective uptime against the swarm = ~1 dmg/sec/ring.
            dps += static_cast<float>(p.countItem(game::ItemId::OrbitalRing)) * 1.0f;

            // Hail Ring: 35 + 18(N-1) syringes per 5s, ~25% land near an enemy.
            const int hail = p.countItem(game::ItemId::HailRing);
            if (hail > 0) {
                const int sy = 35 + 18 * (hail - 1);
                dps += static_cast<float>(sy) * 0.25f / 5.0f;
            }

            // Explosive Auto: 2.5/2^(N-1) sec interval, ~5.5 dmg per shot
            // (1 direct + 3 AoE * ~1.5 enemies hit on average).
            const int explo = p.countItem(game::ItemId::ExplosiveAuto);
            if (explo > 0) {
                const float interval = 2.5f / std::pow(2.0f, static_cast<float>(explo - 1));
                dps += 5.5f / interval;
            }

            // Lightning Ring: same interval, 1 direct + 3 chain = 4 dmg per shot.
            const int light = p.countItem(game::ItemId::LightningRing);
            if (light > 0) {
                const float interval = 2.5f / std::pow(2.0f, static_cast<float>(light - 1));
                dps += 4.0f / interval;
            }
            return dps;
        };

        auto computeHpMultiplier = [&](const game::Player& p,
                                       const game::WaveDef* wave) -> float {
            if (!wave || wave->duration <= 0.0f) return 1.0f;
            const float dps   = playerExpectedDps(p);
            const float total = dps * wave->duration;            // damage budget
            float baseHp = 0.0f;
            for (const auto& g : wave->groups) {
                for (const auto& entry : g.entries) {
                    const int idx = spawner.findDefIndex(entry.name);
                    if (idx >= 0) baseHp += static_cast<float>(entry.count) *
                                            static_cast<float>(enemyDefs[idx].maxHp);
                }
            }
            if (baseHp < 1.0f) return 1.0f;
            const float target = total * kHpScaleRatio;
            const float mult   = target / baseHp;
            return std::clamp(mult, 1.0f, kHpScaleMaxMult);
        };

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

        std::vector<game::AntidoteBox> antidoteBoxes;
        antidoteBoxes.reserve(4);
        int antidoteSpawnedForWave = -1;

        float ringCooldown = 0.0f;

        // Achievement-style loot toast: each chest opening grants the item
        // immediately and pushes a notification that slides in from the
        // top-left, holds for a few seconds, then fades out. No scene change,
        // no input gate — gameplay continues underneath.
        struct LootToast {
            game::ItemInstance item;
            float age = 0.0f;
        };
        constexpr float kToastLifetime = 4.0f;   // seconds before it disappears
        constexpr float kToastSlideIn  = 0.25f;  // seconds for the slide-in
        constexpr float kToastFadeOut  = 0.5f;   // seconds for the fade-out tail
        std::vector<LootToast> lootToasts;

        bool prevRestartKey   = false;

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

        // Hail ring: every kHailInterval seconds, rains kHailBaseCount + 2 per
        // stack syringes within kHailRadius of the player from kHailHeight up.
        constexpr float kHailInterval = 5.0f;   // seconds between hails
        constexpr float kHailRadius   = 10.0f;  // metres of horizontal scatter
        constexpr float kHailHeight   = 18.0f;  // spawn altitude above terrain
        constexpr float kHailFallSpeed = 22.0f; // m/s downward
        constexpr int   kHailBaseCount = 35;    // syringes per hail at 1 stack
        constexpr int   kHailPerStack  = 18;    // additional syringes per extra stack
        float hailCooldown = 1.5f;              // first hail comes a bit early

        // Explosive auto: independent auto-firing weapon. Each shot bursts on
        // impact, dealing AoE damage and emitting a red-orange particle puff.
        // Base cooldown 2.5s; each additional stack halves it.
        constexpr float kExplosiveBaseCooldown = 2.5f;
        constexpr float kExplosiveRadius       = 2.8f;
        constexpr int   kExplosiveDamage       = 3;     // AoE damage per burst
        float explosiveCooldown = 0.5f;                 // first shot fires shortly after start

        // Lightning ring: same fire timing as explosive. On enemy hit, chains
        // to the kLightningChainCount nearest other enemies, dealing a
        // smaller damage to each. Visualised as a vibrant cyan-blue arc.
        constexpr float kLightningBaseCooldown = 2.5f;
        constexpr int   kLightningChainCount   = 3;
        constexpr int   kLightningChainDamage  = 1;
        constexpr glm::vec3 kLightningTint(0.40f, 0.85f, 1.0f);  // bright cyan-blue
        float lightningCooldown = 0.5f;

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
        render::Arrows arrows;
        if (!arrows.init("assets/shaders/arrow.vert",
                         "assets/shaders/arrow.frag")) {
            return 1;
        }
        render::Minimap minimap;
        if (!minimap.init("assets/shaders/minimap_disc.vert",
                          "assets/shaders/minimap_disc.frag")) {
            return 1;
        }
        render::Skybox skybox;
        if (!skybox.init("assets/shaders/sky.vert",
                         "assets/shaders/sky.frag",
                         "assets/textures/satara_night_1k.hdr")) {
            std::fprintf(stderr, "[main] skybox failed to load — continuing without one\n");
        }

        game::StartMenu     startMenu;
        startMenu.load("data/player_brief.txt");
        game::SettingsMenu  settingsMenu;
        game::LevelUpMenu   levelUpMenu;
        game::StatsScreen   statsScreen;
        game::JournalScreen journalScreen;
        journalScreen.load("data/Journal.csv");
        game::Scene scene = game::Scene::StartMenu;
        bool prevEscape   = false;
        bool prevB        = false;
        bool prevJ        = false;

        // Minimap state. M toggles between compact (top-right) and an
        // enlarged centred view. World radius covers a fixed area regardless
        // of size, so the enlarged map = same area at higher zoom.
        bool prevM            = false;
        bool minimapExpanded  = false;
        constexpr float kMinimapRadiusPx        = 110.0f;  // compact pixel radius
        constexpr float kMinimapExpandedPx      = 320.0f;  // M-toggle pixel radius
        constexpr float kMinimapWorldRadius     = 35.0f;   // metres covered by the disc
        constexpr float kMinimapMargin          = 24.0f;   // distance from screen corner

        glClearColor(0.02f, 0.02f, 0.03f, 1.0f);

        while (!window.shouldClose()) {
            window.pollEvents();
            input.update();
            time.tick();
            float dt = time.dt();

            // Enforce cursor mode every frame against the current scene. The
            // per-transition toggles below are still useful for resetMouseDelta,
            // but this guard ensures the cursor never gets stuck hidden if a
            // transition is missed (alt-tab, scene auto-changes, etc.).
            {
                const int desired = (scene == game::Scene::Playing)
                                       ? GLFW_CURSOR_DISABLED
                                       : GLFW_CURSOR_NORMAL;
                if (glfwGetInputMode(window.handle(), GLFW_CURSOR) != desired) {
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, desired);
                    input.resetMouseDelta();
                }
            }

            // Escape edge-toggles the settings overlay; it no longer quits.
            // Disabled on the start menu so Escape can't skip the briefing.
            const bool curEscape = input.key(GLFW_KEY_ESCAPE);
            if (curEscape && !prevEscape && scene != game::Scene::StartMenu) {
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

            // J toggles the journal while playing.
            const bool curJ = input.key(GLFW_KEY_J);
            if (curJ && !prevJ) {
                if (scene == game::Scene::Playing) {
                    journalScreen.open();
                    scene = game::Scene::Journal;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    input.resetMouseDelta();
                } else if (scene == game::Scene::Journal) {
                    scene = game::Scene::Playing;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    input.resetMouseDelta();
                }
            }
            prevJ = curJ;

            // M edge-toggles the enlarged minimap. Compact (top-right) is
            // always shown; M just swaps to a centred zoomed-out version
            // without pausing the game.
            const bool curM = input.key(GLFW_KEY_M);
            if (curM && !prevM) {
                minimapExpanded = !minimapExpanded;
            }
            prevM = curM;

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

            // Master volume: settings stores 0..100, miniaudio wants 0..1.
            audio.setMasterVolume(settingsMenu.settings().masterVolume * 0.01f);

            if (scene == game::Scene::StartMenu) {
                if (startMenu.update(input, window.width(), window.height())) {
                    scene = game::Scene::Playing;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    input.resetMouseDelta();
                }
            } else if (scene == game::Scene::Playing) {
                // Trigger level-up menu if pending.
                if (player.pendingLevelUps() > 0) {
                    scene = game::Scene::LevelUp;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    input.resetMouseDelta();
                    levelUpMenu.reset();
                } else {
                    float gh = terrain.heightAt(player.position().x, player.position().z);
                    player.update(dt, input, time.total(), gh);
                    // Keep the player inside the perimeter walls.
                    const float bound = kWallHalfMap - kPlayerInset;
                    player.clampXZ(-bound, bound, -bound, bound);
                    weapon.fireRate = 1.0f / player.attackSpeed;
                    weapon.update(dt, input, player);

                    // Footsteps: gate gain by horizontal speed; pitch up
                    // toward sprint speed so the clip cadence keeps up.
                    {
                        const glm::vec3 v = player.velocity();
                        const float speedXZ  = std::sqrt(v.x * v.x + v.z * v.z);
                        const float walk     = player.feel().moveSpeed;
                        const float sprint   = walk * player.feel().sprintMultiplier;
                        const float kStepMin = 0.5f;        // m/s — below this we treat as standing still
                        const float target   = (speedXZ > kStepMin) ? 1.0f : 0.0f;
                        const float blend    = std::min(8.0f * dt, 1.0f);
                        footstepGain = glm::mix(footstepGain, target, blend);

                        const float t01 =
                            std::clamp((speedXZ - walk) / std::max(0.001f, sprint - walk),
                                       0.0f, 1.0f);
                        const float pitch = 1.0f + 0.4f * t01;  // 1.0 at walk, 1.4 at sprint

                        audio.setVolume("footsteps", footstepGain);
                        audio.setPitch ("footsteps", pitch);
                        if (footstepGain > 0.005f) audio.start("footsteps");
                        else                       audio.stop ("footsteps");
                    }
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
            } else if (scene == game::Scene::Journal) {
                journalScreen.update(input, window.width(), window.height());
            }
            // Hard-stop loops when the player isn't actually moving through
            // the world (any non-Playing scene). Ambient runs whenever
            // Playing is active so the crackle ducks under the menus.
            if (scene == game::Scene::Playing) {
                audio.start("ambient");
                audio.start("ambient2");
            } else {
                footstepGain = 0.0f;
                audio.stop("footsteps");
                audio.stop("ambient");
                audio.stop("ambient2");
            }
            journalScreen.tickToast(dt);

            // Apply fullscreen toggle each frame (cheap if already in state).
            window.setFullscreen(settingsMenu.settings().fullscreen);

            const render::Camera& cam = player.camera();

            // Update miniaudio's 3D listener with the camera pose so that
            // positional one-shots (enemy footsteps) attenuate and pan
            // correctly. Y-up world; forward derived from camera yaw/pitch.
            {
                const glm::vec3 fwd = cam.forward();
                const float lpos[3] = { cam.position.x, cam.position.y, cam.position.z };
                const float lfwd[3] = { fwd.x,          fwd.y,          fwd.z          };
                const float lup [3] = { 0.0f,           1.0f,           0.0f           };
                audio.setListener(lpos, lfwd, lup);
            }

            // Projectile spawn / advance only run while playing; the menu pauses
            // the world.
            if (scene == game::Scene::Playing) {
            // Spawn a projectile on the fire frame.
            if (weapon.firedThisFrame()) {
                audio.playOneShot("assets/audio/gun_emission.mp3");
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

            // Advance and cull projectiles. Stuck syringes don't integrate
            // velocity — their position is driven each frame from the host
            // enemy after the enemy update pass.
            for (auto& p : projectiles) {
                if (!p.stuck()) p.position += p.velocity * dt;
                p.age += dt;
            }
            // Sticky projectiles (hail) snap to the terrain on first contact
            // and freeze in place. Their existing maxAge keeps them visible
            // for a beat afterwards, so the field looks pin-cushioned.
            for (auto& p : projectiles) {
                if (!p.sticky) continue;
                if (p.velocity.y >= 0.0f) continue;  // already stuck or rising
                const float gh = terrain.heightAt(p.position.x, p.position.z);
                if (p.position.y <= gh) {
                    p.position.y = gh;
                    p.velocity   = glm::vec3(0.0f);
                    // Linger for a few seconds where it landed.
                    p.maxAge     = p.age + 4.0f;
                }
            }
            // Spawn + advance enemies (also gated by Playing scene).
            // WaveManager owns scheduling now; falls back to the legacy spawner
            // tick only if no waves were loaded.
            if (waveManager.totalWaves() > 0) {
                // Refresh the difficulty multiplier each frame so groups that
                // spawn later in a wave reflect any items the player just
                // grabbed mid-fight.
                spawner.setHpMultiplier(
                    computeHpMultiplier(player, waveManager.currentWave()));
                waveManager.update(dt, enemies, player.position());

                // Spawn one antidote box at the start of each new wave.
                // Try a few angles and clamp into the wall-bounded area so
                // the box is always reachable.
                const int curWave = waveManager.currentWaveIndex();
                if (!waveManager.finished() && curWave != antidoteSpawnedForWave) {
                    // Reward player with a 20% max-health heal when a wave
                    // advances. Skipped on the very first wave (-1 -> 0)
                    // since nothing has been completed yet.
                    if (antidoteSpawnedForWave >= 0 && curWave > antidoteSpawnedForWave) {
                        player.health = std::min(player.maxHealth,
                                                 player.health + 0.20f * player.maxHealth);
                    }
                    antidoteSpawnedForWave = curWave;
                    journalScreen.unlock();
                    antidoteBoxes.clear();
                    // Inset accounts for the box's draw scale so the visual
                    // never sticks through the wall geometry.
                    const float boxBound = kWallHalfMap - kPlayerInset - 5.0f;
                    float spx = 0.0f, spz = 0.0f;
                    for (int attempt = 0; attempt < 6; ++attempt) {
                        const float angle = game::rand01() * 6.28318530718f;
                        spx = player.position().x + std::cos(angle) * game::kAntidoteSpawnDist;
                        spz = player.position().z + std::sin(angle) * game::kAntidoteSpawnDist;
                        if (std::abs(spx) <= boxBound && std::abs(spz) <= boxBound) break;
                    }
                    // Final clamp: even if every angle landed outside (player
                    // standing in a corner), force the box back inside the walls.
                    spx = std::clamp(spx, -boxBound, boxBound);
                    spz = std::clamp(spz, -boxBound, boxBound);
                    game::AntidoteBox box;
                    box.position = glm::vec3(spx, terrain.heightAt(spx, spz), spz);
                    box.yawRad   = game::rand01() * 6.28318530718f;
                    antidoteBoxes.push_back(box);
                }
                // Wave timer expired without antidote collected — game over.
                if (waveManager.waitingForAntidote()) {
                    player.health = 0.0f;
                    scene = game::Scene::GameOver;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    input.resetMouseDelta();
                }
                // All waves complete — victory.
                if (waveManager.finished() && scene == game::Scene::Playing) {
                    scene = game::Scene::Victory;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    input.resetMouseDelta();
                }
            } else {
                spawner.update(dt, enemies, player.position());
            }

            // Antidote box: check player proximity for collection.
            for (auto& box : antidoteBoxes) {
                if (box.state != game::AntidoteBoxState::Active) continue;
                const float dx = box.position.x - player.position().x;
                const float dz = box.position.z - player.position().z;
                if (dx * dx + dz * dz < game::kAntidotePickupRadius * game::kAntidotePickupRadius) {
                    box.state = game::AntidoteBoxState::Collected;
                    waveManager.setAntidoteCollected(true);
                }
            }
            antidoteBoxes.erase(
                std::remove_if(antidoteBoxes.begin(), antidoteBoxes.end(),
                    [](const game::AntidoteBox& b){ return b.state == game::AntidoteBoxState::Collected; }),
                antidoteBoxes.end());

            // Beacon to the sky — a dense vertical pillar of bright green
            // particles rising 40m above each active box. Each frame we
            // emit a column of short-lived sparks evenly spaced along the
            // height; constant respawn rate makes it read as a beam.
            {
                constexpr float kBeamHeight     = 40.0f;
                constexpr int   kBeamPartsPerBox = 90;     // particles emitted per box per frame
                constexpr float kBeamRadius     = 0.09f;   // horizontal jitter (metres)
                for (const auto& box : antidoteBoxes) {
                    if (box.state != game::AntidoteBoxState::Active) continue;
                    const float gh = terrain.heightAt(box.position.x, box.position.z);
                    const float baseY = gh + game::kAntidoteYOffset + 0.2f;
                    for (int i = 0; i < kBeamPartsPerBox; ++i) {
                        const float t01   = game::rand01();          // height fraction
                        const float az    = game::rand01() * 6.28318530718f;
                        const float r     = std::sqrt(game::rand01()) * kBeamRadius;
                        render::Particle pp;
                        pp.position = glm::vec3(box.position.x + std::cos(az) * r,
                                                baseY + t01 * kBeamHeight,
                                                box.position.z + std::sin(az) * r);
                        // Slow upward drift; keeps the column alive briefly.
                        pp.velocity = glm::vec3(0.0f,
                                                0.5f + game::rand01() * 0.4f,
                                                0.0f);
                        const float ct = game::rand01();
                        // Hot core toward white, cooler edges toward green.
                        pp.color = glm::vec4(0.55f + ct * 0.45f,
                                             1.0f,
                                             0.55f + ct * 0.30f,
                                             1.0f);
                        pp.life  = 0.18f + game::rand01() * 0.10f;   // short — beam stays "still"
                        pp.age   = 0.0f;
                        // pp.size feeds gl_PointSize via aSize*viewportH/clip.w
                        // in particle.vert. The GPU clamps gl_PointSize at
                        // GL_POINT_SIZE_MAX (often ~64-256 px), so any
                        // "size" big enough to land above the clamp at 30 m
                        // distance produces an identical-looking blob no
                        // matter how much you shrink it. Keep aSize sub-1
                        // so the formula stays under the clamp and actually
                        // honours our radius.
                        pp.size  = 0.55f - t01 * 0.20f + game::rand01() * 0.10f;
                        particles.emit(pp);
                    }

                }
            }
            // Pathfinding is XZ only (handled in Enemy::update). Y is forced
            // to the terrain height under each enemy every frame so the model
            // tracks the surface and never clips into hills.
            for (auto& e : enemies) {
                if (e.alive())
                    e.position.y = terrain.heightAt(e.position.x, e.position.z);
            }

            for (auto& e : enemies) {
                if (!e.def) continue;
                if (e.alive()) {
                    // Aggro check: within def->aggroRange of the player, swap
                    // to the def's aggroAnim and apply the speed multiplier.
                    if (e.def->aggroAnim && e.def->aggroRange > 0.0f) {
                        const float dx = e.position.x - player.position().x;
                        const float dz = e.position.z - player.position().z;
                        const bool aggro =
                            (dx * dx + dz * dz) < (e.def->aggroRange * e.def->aggroRange);
                        const render::AnimationClip* desired =
                            aggro ? e.def->aggroAnim : e.def->walkAnim;
                        if (desired != e.currentAnim) {
                            e.animator.setAnimation(desired);
                            e.currentAnim = desired;
                        }
                        e.speedMult = aggro ? e.def->aggroSpeedMult : 1.0f;
                    }
                    e.update(dt, player.position());

                    // Footstep one-shot per stride. Cadence is one step
                    // per ~0.7 m of travel, clamped so a sprinting cat
                    // can't machine-gun its footfalls. Volume falls off
                    // automatically via miniaudio's linear attenuation
                    // (min=2 m, max=28 m) — set in Audio::playPositional.
                    e.stepTimer -= dt;
                    if (e.stepTimer <= 0.0f) {
                        const float speedXZ = std::sqrt(e.velocity.x * e.velocity.x +
                                                        e.velocity.z * e.velocity.z);
                        if (speedXZ > 0.1f) {
                            const float pos[3] = { e.position.x, e.position.y, e.position.z };
                            audio.playPositional("assets/audio/enemy_footsteps.mp3", pos, 0.45f);
                            const float interval = std::max(0.18f, 0.7f / speedXZ);
                            e.stepTimer = interval;
                        } else {
                            e.stepTimer = 0.2f;  // re-check soon
                        }
                    }
                }
                // Tick animator + bones every frame so death animations play
                // out even though hp == 0 (alive() == false).
                e.animator.update(dt);
                if (e.def->model->skeleton()) {
                    e.animator.calculateBoneTransforms(&e.def->model->skeleton().value());
                }
            }

            // Stuck syringes follow their host enemy. Linear scan keyed by
            // enemy id is fine — both lists are small. If the host has been
            // culled (death animation finished) the projectile keeps its
            // last position and ages out normally.
            if (!projectiles.empty() && !enemies.empty()) {
                for (auto& p : projectiles) {
                    if (!p.stuck()) continue;
                    for (const auto& e : enemies) {
                        if (e.id == p.stuckEnemyId) {
                            p.position = e.position + p.stuckOffset;
                            break;
                        }
                    }
                }
            }

            // Enemy contact damage. XZ distance check + per-enemy cooldown so
            // a clustered swarm can't burst the player in one frame.
            for (auto& e : enemies) {
                if (!e.alive() || !e.def) continue;
                if (e.attackCooldown > 0.0f) continue;
                const glm::vec3 d = e.position - player.position();
                const float xzDist = std::sqrt(d.x * d.x + d.z * d.z);
                if (xzDist < e.def->attackRange) {
                    player.health = std::max(0.0f, player.health - e.def->damage);
                    player.addTrauma(0.35f);
                    player.applyKnockback(player.position() - e.position);
                    e.attackCooldown = e.def->attackInterval;
                }
            }

            // Player death — switch to GameOver screen.
            if (player.health <= 0.0f) {
                player.health = 0.0f;
                scene = game::Scene::GameOver;
                glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                input.resetMouseDelta();
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
                            pj.position        = spawn;
                            pj.velocity        = dir * projectileSpeed;
                            pj.scale           = projectileScale;
                            pj.maxAge          = 3.0f;
                            projectiles.push_back(pj);
                            audio.playOneShot("assets/audio/gun_emission.mp3");
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

            // Explosive Auto ring: independent auto-fire weapon. Same spawn
            // offset as Auto Ring (held-syringe tip), aimed at the nearest
            // enemy. Each shot bursts on impact (red-orange particles + AoE).
            // Cooldown halves with every additional stack.
            {
                const int explosiveStacks = player.countItem(game::ItemId::ExplosiveAuto);
                if (explosiveStacks > 0) {
                    explosiveCooldown -= dt;
                    if (explosiveCooldown <= 0.0f) {
                        const float kRange = 25.0f;
                        game::Enemy* target = nullptr;
                        float bestD2 = kRange * kRange;
                        for (auto& e : enemies) {
                            if (!e.alive()) continue;
                            glm::vec3 d = e.hitCentre() - cam.position;
                            float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                            if (d2 < bestD2) { bestD2 = d2; target = &e; }
                        }
                        if (target) {
                            glm::vec3 baseFwd   = cam.forward();
                            glm::vec3 baseRight = glm::normalize(glm::cross(baseFwd, glm::vec3(0,1,0)));
                            glm::vec3 up        = glm::cross(baseRight, baseFwd);
                            glm::vec3 spawn = cam.position
                                            + baseFwd  * 0.45f
                                            + baseRight * 0.18f
                                            - up        * 0.45f;
                            glm::vec3 dir = glm::normalize(target->hitCentre() - spawn);

                            game::Projectile pj;
                            pj.position        = spawn;
                            pj.velocity        = dir * projectileSpeed;
                            pj.scale           = projectileScale;
                            pj.maxAge          = 3.0f;
                            pj.explosiveDamage = kExplosiveDamage;  // > 0 marks it as explosive
                            projectiles.push_back(pj);
                            audio.playOneShot("assets/audio/gun_emission.mp3");

                            // 2.5s, halved per additional stack.
                            explosiveCooldown =
                                kExplosiveBaseCooldown /
                                std::pow(2.0f, static_cast<float>(explosiveStacks - 1));
                        } else {
                            explosiveCooldown = 0.25f;
                        }
                    }
                } else {
                    explosiveCooldown = 0.0f;
                }
            }

            // Lightning ring: same fire timing as Explosive. On hit, chains
            // damage to the kLightningChainCount nearest other enemies and
            // draws a vibrant cyan-blue arc to each.
            {
                const int lightningStacks = player.countItem(game::ItemId::LightningRing);
                if (lightningStacks > 0) {
                    lightningCooldown -= dt;
                    if (lightningCooldown <= 0.0f) {
                        const float kRange = 25.0f;
                        game::Enemy* target = nullptr;
                        float bestD2 = kRange * kRange;
                        for (auto& e : enemies) {
                            if (!e.alive()) continue;
                            glm::vec3 d = e.hitCentre() - cam.position;
                            float d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                            if (d2 < bestD2) { bestD2 = d2; target = &e; }
                        }
                        if (target) {
                            glm::vec3 baseFwd   = cam.forward();
                            glm::vec3 baseRight = glm::normalize(glm::cross(baseFwd, glm::vec3(0,1,0)));
                            glm::vec3 up        = glm::cross(baseRight, baseFwd);
                            glm::vec3 spawn = cam.position
                                            + baseFwd  * 0.45f
                                            + baseRight * 0.18f
                                            - up        * 0.45f;
                            glm::vec3 dir = glm::normalize(target->hitCentre() - spawn);

                            game::Projectile pj;
                            pj.position        = spawn;
                            pj.velocity        = dir * projectileSpeed;
                            pj.scale           = projectileScale;
                            pj.maxAge          = 3.0f;
                            pj.lightningDamage = kLightningChainDamage;  // > 0 marks it as lightning
                            projectiles.push_back(pj);
                            audio.playOneShot("assets/audio/gun_emission.mp3");

                            lightningCooldown =
                                kLightningBaseCooldown /
                                std::pow(2.0f, static_cast<float>(lightningStacks - 1));
                        } else {
                            lightningCooldown = 0.25f;
                        }
                    }
                } else {
                    lightningCooldown = 0.0f;
                }
            }

            // Hail ring: every kHailInterval, rains syringes within kHailRadius.
            {
                const int hailStacks = player.countItem(game::ItemId::HailRing);
                if (hailStacks > 0) {
                    hailCooldown -= dt;
                    if (hailCooldown <= 0.0f) {
                        hailCooldown += kHailInterval;
                        const int count = kHailBaseCount + kHailPerStack * (hailStacks - 1);
                        const float gh = terrain.heightAt(player.position().x,
                                                          player.position().z);
                        for (int i = 0; i < count; ++i) {
                            // Uniform-area sample inside the hail radius.
                            const float angle = game::rand01() * 6.28318530718f;
                            const float r     = std::sqrt(game::rand01()) * kHailRadius;
                            game::Projectile pj;
                            pj.position = glm::vec3(player.position().x + std::cos(angle) * r,
                                                    gh + kHailHeight,
                                                    player.position().z + std::sin(angle) * r);
                            pj.velocity = glm::vec3(0.0f, -kHailFallSpeed, 0.0f);
                            pj.scale    = projectileScale * 2.2f;  // chunky so the storm reads at distance (4.0 / 1.8)
                            // Long lifetime: enough for the fall + the stick.
                            // Will be re-clamped by the terrain-snap pass below
                            // once the syringe lands.
                            pj.maxAge   = 8.0f;
                            pj.sticky   = true;
                            projectiles.push_back(pj);
                        }
                    }
                } else {
                    hailCooldown = kHailInterval;
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
                                    const float drop = e.def ? e.def->dropChance : game::kChestDropChance;
                                    if (game::rand01() < drop) {
                                        game::Chest c;
                                        c.position   = e.position;
                                        c.yawRad     = game::rand01() * 6.28318530718f;
                                        c.rolled     = game::rollChestRarity();
                                        c.itemId     = game::rollChestItem(c.rolled);
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
            // Helper: rolls a chest drop on enemy death + grants XP. Used by
            // both the direct-hit path and the explosive-AoE path so kills
            // from either route reward identically.
            auto onEnemyKilled = [&](game::Enemy& dead) {
                player.addXp(game::kEnemyXpReward);
                const float drop = dead.def ? dead.def->dropChance : game::kChestDropChance;
                if (game::rand01() < drop) {
                    game::Chest c;
                    c.position   = dead.position;
                    c.yawRad     = game::rand01() * 6.28318530718f;
                    c.rolled     = game::rollChestRarity();
                    c.itemId     = game::rollChestItem(c.rolled);
                    c.state      = game::ChestState::Opening;
                    c.openTimer  = 0.0f;
                    c.cyclePhase = 1.0f;
                    c.animator.setAnimation(chestOpenAnim, /*loop=*/false);
                    chests.push_back(c);
                }
                // Death animation: linger as a corpse for the clip's duration,
                // playing the death anim once. The cull pass below keeps the
                // entity around until deathTimer >= deathDuration.
                if (dead.def && dead.def->deathAnim) {
                    dead.dying         = true;
                    dead.deathTimer    = 0.0f;
                    dead.deathDuration = dead.def->deathAnim->duration;
                    dead.animator.setAnimation(dead.def->deathAnim, /*loop=*/false);
                    dead.currentAnim   = dead.def->deathAnim;
                    dead.speedMult     = 0.0f;  // freeze in place
                }
            };

            for (auto& p : projectiles) {
                if (!p.alive()) continue;
                if (p.stuck())  continue;  // already embedded in an enemy
                for (auto& e : enemies) {
                    if (!e.alive()) continue;
                    float hitRadius = e.def ? e.def->radius : game::kEnemyRadius;
                    if (glm::distance(p.position, e.hitCentre()) < hitRadius) {
                        e.hp -= 1;
                        const glm::vec3 impact = p.position;
                        const int explosiveDmg = p.explosiveDamage;
                        // Embed the syringe in the enemy. Velocity is
                        // preserved so the projectile draw pass keeps
                        // orienting the model along the way it came in;
                        // the stuck-follow loop drives position from now
                        // on. Origin at the impact point puts the model's
                        // tip just inside the enemy and the body sticks
                        // out behind (along reverse-velocity).
                        p.stuckEnemyId = e.id;
                        p.stuckOffset  = impact - e.position;
                        p.age          = 0.0f;
                        p.maxAge       = 4.0f;     // linger time once embedded
                        if (!e.alive()) {
                            onEnemyKilled(e);
                        }

                        // Blood spatter on every syringe hit. Particles are
                        // additively blended so multiple overlapping reds
                        // brighten toward pink — small droplets, dim
                        // alpha. NOTE: pp.size is "pixels at clip-w=1",
                        // so 0.08 -> ~10 px at 10 m. The chest/explosive
                        // particles use much larger sizes because they
                        // intentionally read as a wash; blood needs to
                        // look like discrete droplets.
                        {
                            const int kDropCount = 14;
                            for (int i = 0; i < kDropCount; ++i) {
                                const float az = game::rand01() * 6.28318530718f;
                                const float el = (game::rand01() * 0.9f) - 0.4f;  // -0.4..0.5 rad
                                const float spd = 2.0f + game::rand01() * 3.0f;
                                const glm::vec3 vel(std::cos(az) * std::cos(el) * spd,
                                                    std::sin(el) * spd - 0.6f,
                                                    std::sin(az) * std::cos(el) * spd);
                                const float t = game::rand01();
                                render::Particle pp;
                                pp.position = impact;
                                pp.velocity = vel;
                                pp.color = glm::vec4(0.55f + t * 0.30f,
                                                     0.00f + t * 0.05f,
                                                     0.00f + t * 0.03f,
                                                     0.55f);
                                pp.life = 0.40f + game::rand01() * 0.25f;
                                pp.age  = 0.0f;
                                pp.size = 0.06f + game::rand01() * 0.08f;  // 0.06..0.14
                                particles.emit(pp);
                            }
                        }
                        // Explosive auto: AoE damage + bright red-orange burst
                        // particle puff at impact. The directly-hit enemy
                        // already took the base 1 dmg above; AoE pass skips
                        // them to avoid double-counting.
                        if (explosiveDmg > 0) {
                            for (auto& other : enemies) {
                                if (!other.alive()) continue;
                                if (&other == &e) continue;
                                if (glm::distance(impact, other.hitCentre()) <= kExplosiveRadius) {
                                    other.hp -= explosiveDmg;
                                    if (!other.alive()) onEnemyKilled(other);
                                }
                            }
                            // Visual ignition — ~70 particles, hemisphere bias
                            // upward + outward, bright red-orange fire palette.
                            const int kBurstCount = 70;
                            for (int i = 0; i < kBurstCount; ++i) {
                                const float az = game::rand01() * 6.28318530718f;
                                const float el = game::rand01() * 1.10f + 0.10f;  // 0.10..1.20 rad up
                                const float spd = 4.0f + game::rand01() * 5.0f;
                                const glm::vec3 vel(std::cos(az) * std::cos(el) * spd,
                                                    std::sin(el) * spd,
                                                    std::sin(az) * std::cos(el) * spd);
                                const float t = game::rand01();
                                render::Particle pp;
                                pp.position = impact;
                                pp.velocity = vel;
                                // Mix from hot yellow-orange to deep red.
                                pp.color = glm::vec4(1.0f,
                                                     0.25f + t * 0.45f,
                                                     0.05f + t * 0.10f,
                                                     1.0f);
                                pp.life = 0.55f + game::rand01() * 0.35f;
                                pp.age  = 0.0f;
                                pp.size = 28.0f + game::rand01() * 18.0f;
                                particles.emit(pp);
                            }
                        }

                        // Lightning ring: chain to the N nearest other enemies
                        // and draw a vibrant cyan-blue arc to each.
                        if (p.lightningDamage > 0) {
                            // Build a list of other alive enemies sorted by
                            // distance to the impact, take up to N.
                            struct ChainCandidate { float d2; game::Enemy* en; };
                            std::vector<ChainCandidate> cands;
                            cands.reserve(enemies.size());
                            for (auto& other : enemies) {
                                if (!other.alive()) continue;
                                if (&other == &e) continue;
                                glm::vec3 d = other.hitCentre() - impact;
                                cands.push_back({ d.x*d.x + d.y*d.y + d.z*d.z, &other });
                            }
                            std::sort(cands.begin(), cands.end(),
                                [](const ChainCandidate& a, const ChainCandidate& b){
                                    return a.d2 < b.d2;
                                });
                            const int hits = std::min(kLightningChainCount,
                                                       static_cast<int>(cands.size()));
                            for (int k = 0; k < hits; ++k) {
                                ChainCandidate& c = cands[k];
                                c.en->hp -= p.lightningDamage;
                                if (!c.en->alive()) onEnemyKilled(*c.en);

                                // Particle arc — many small bright cyan
                                // particles strung along a jittered line so
                                // it reads as a flickering bolt.
                                const glm::vec3 a = impact;
                                const glm::vec3 b = c.en->hitCentre();
                                const glm::vec3 ab = b - a;
                                const float len = glm::length(ab);
                                if (len < 1e-3f) continue;
                                const glm::vec3 dir = ab / len;
                                // Perpendicular axes for the jagged offset.
                                const glm::vec3 ref = std::abs(dir.y) > 0.9f
                                                        ? glm::vec3(1, 0, 0)
                                                        : glm::vec3(0, 1, 0);
                                const glm::vec3 perp1 = glm::normalize(glm::cross(dir, ref));
                                const glm::vec3 perp2 = glm::cross(dir, perp1);
                                const int segs = std::clamp(int(len * 3.0f), 12, 80);
                                const float jitter = std::min(0.18f, len * 0.04f);
                                for (int s = 0; s < segs; ++s) {
                                    const float t = static_cast<float>(s) / (segs - 1);
                                    const float jx = (game::rand01() - 0.5f) * 2.0f * jitter;
                                    const float jy = (game::rand01() - 0.5f) * 2.0f * jitter;
                                    glm::vec3 pos = a + ab * t + perp1 * jx + perp2 * jy;
                                    render::Particle pp;
                                    pp.position = pos;
                                    pp.velocity = glm::vec3(0.0f);
                                    pp.color = glm::vec4(kLightningTint.x,
                                                         kLightningTint.y,
                                                         kLightningTint.z,
                                                         1.0f);
                                    pp.life = 0.18f + game::rand01() * 0.12f;
                                    pp.age  = 0.0f;
                                    pp.size = 18.0f + game::rand01() * 10.0f;
                                    particles.emit(pp);
                                }
                                // White-hot sparkle at the chained enemy.
                                for (int s = 0; s < 14; ++s) {
                                    const float az = game::rand01() * 6.28318530718f;
                                    const float el = game::rand01() * 1.5f - 0.2f;
                                    const float spd = 2.5f + game::rand01() * 3.0f;
                                    render::Particle pp;
                                    pp.position = b;
                                    pp.velocity = glm::vec3(std::cos(az) * std::cos(el) * spd,
                                                            std::sin(el) * spd,
                                                            std::sin(az) * std::cos(el) * spd);
                                    pp.color = glm::vec4(0.85f, 0.95f, 1.0f, 1.0f);
                                    pp.life = 0.30f + game::rand01() * 0.20f;
                                    pp.age  = 0.0f;
                                    pp.size = 22.0f + game::rand01() * 12.0f;
                                    particles.emit(pp);
                                }
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
                        // Grant immediately + queue a top-left toast.
                        const game::ItemInstance got { c.itemId, c.rolled };
                        player.grantItem(got);
                        lootToasts.push_back({ got, 0.0f });
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

            // Tick the death-animation timer for dying enemies.
            for (auto& e : enemies) {
                if (e.dying) e.deathTimer += dt;
            }
            // Cull: dead AND not in the middle of a death-anim window.
            enemies.erase(
                std::remove_if(enemies.begin(), enemies.end(),
                    [](const game::Enemy& e) {
                        if (e.alive()) return false;
                        if (e.dying && e.deathTimer < e.deathDuration) return false;
                        return true;
                    }),
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

            // Tick the bats' flap animations every frame (regardless of scene
            // so they keep flying behind menus/loot popups/etc).
            if (batModel.skeleton()) {
                for (auto& b : bats) {
                    b.animator.update(dt);
                    b.animator.calculateBoneTransforms(&batModel.skeleton().value());
                }
            }

            // Tick loot toasts (always, regardless of scene) and cull the
            // ones whose lifetime has elapsed.
            for (auto& t : lootToasts) t.age += dt;
            lootToasts.erase(
                std::remove_if(lootToasts.begin(), lootToasts.end(),
                    [&](const LootToast& t){ return t.age >= kToastLifetime; }),
                lootToasts.end());

            // GameOver: R key (or Enter) restarts everything from scratch.
            if (scene == game::Scene::GameOver) {
                const bool curR = input.key(GLFW_KEY_R) || input.key(GLFW_KEY_ENTER);
                const bool justPressed = curR && !prevRestartKey;
                prevRestartKey = curR;
                if (justPressed) {
                    player.restart();
                    player.setSpawn({ 0.0f, terrain.heightAt(0.0f, 3.0f), 3.0f });
                    weapon.projectileCount = 1;
                    weapon.fanColumns      = 1;
                    projectileSpeed        = 25.0f;
                    enemies.clear();
                    chests.clear();
                    antidoteBoxes.clear();
                    antidoteSpawnedForWave = -1;
                    projectiles.clear();
                    lootToasts.clear();
                    orbitalCooldowns.clear();
                    orbitalAngle = 0.0f;
                    ringCooldown = 0.0f;
                    waveManager.reset();
                    journalScreen.resetProgress();
                    scene = game::Scene::Playing;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    input.resetMouseDelta();
                }
            }

            // Victory: R key (or Enter) restarts from scratch.
            if (scene == game::Scene::Victory) {
                const bool curR = input.key(GLFW_KEY_R) || input.key(GLFW_KEY_ENTER);
                const bool justPressed = curR && !prevRestartKey;
                prevRestartKey = curR;
                if (justPressed) {
                    player.restart();
                    player.setSpawn({ 0.0f, terrain.heightAt(0.0f, 3.0f), 3.0f });
                    weapon.projectileCount = 1;
                    weapon.fanColumns      = 1;
                    projectileSpeed        = 25.0f;
                    enemies.clear();
                    chests.clear();
                    antidoteBoxes.clear();
                    antidoteSpawnedForWave = -1;
                    projectiles.clear();
                    lootToasts.clear();
                    orbitalCooldowns.clear();
                    orbitalAngle = 0.0f;
                    ringCooldown = 0.0f;
                    waveManager.reset();
                    journalScreen.resetProgress();
                    scene = game::Scene::Playing;
                    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    input.resetMouseDelta();
                }
            }

            // Keep the scene FBO matched to the window size.
            sceneFbo.resize(window.width(), window.height());
            sceneFbo.bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Skybox first — depth-disabled fullscreen pass; subsequent
            // world geometry overdraws wherever it passes depth testing.
            {
                const glm::mat4 viewProj = cam.proj(window.aspect()) * cam.view();
                const glm::mat4 invVP    = glm::inverse(viewProj);
                skybox.draw(invVP, cam.position, /*exposure=*/1.6f);
            }

            worldShader.use();
            checker.bind(0);
            worldShader.setInt  ("uAlbedo", 0);
            worldShader.setInt  ("uHasBones", 0);
            worldShader.setFloat("uAlpha",   1.0f);
            worldShader.setMat4 ("uViewProj", cam.proj(window.aspect()) * cam.view());
            worldShader.setVec3 ("uCamPos", cam.position);
            worldShader.setVec3 ("uCamDir", cam.forward());
            worldShader.setVec3 ("uAmbient", glm::vec3(0.22f));
            {
                const float outerDeg = settingsMenu.settings().flashlightDeg;
                const float innerDeg = outerDeg * 0.6f;  // hot core ~60% of outer
                worldShader.setFloat("uFlashInner",
                                     glm::cos(glm::radians(innerDeg)));
                worldShader.setFloat("uFlashOuter",
                                     glm::cos(glm::radians(outerDeg)));
            }
            worldShader.setVec3 ("uFlashColor", glm::vec3(2.2f, 2.1f, 1.85f));
            worldShader.setVec3 ("uTint", glm::vec3(1.0f));  // default for world geometry

            // Terrain mesh (replaces the old flat floor + cube grid).
            terrain.texture().bind(0);
            worldShader.setMat4("uModel", glm::mat4(1.0f));
            terrain.mesh().draw();
            checker.bind(0); // restore for subsequent geometry

            // Map decorations: trees + tombstones, scattered at startup.
            if (!decorations.empty()) {
                GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
                glDisable(GL_CULL_FACE);  // some GLB winding is inverse
                worldShader.setInt("uHasBones", 0);
                worldShader.setVec3("uTint", glm::vec3(1.0f));
                for (const auto& d : decorations) {
                    glm::mat4 M(1.0f);
                    M = glm::translate(M, d.position);
                    M = glm::rotate(M, d.yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
                    if (d.basePitchDeg != 0.0f) {
                        M = glm::rotate(M, glm::radians(d.basePitchDeg),
                                        glm::vec3(1.0f, 0.0f, 0.0f));
                    }
                    if (d.baseYawDeg != 0.0f) {
                        M = glm::rotate(M, glm::radians(d.baseYawDeg),
                                        glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                    M = glm::scale(M, glm::vec3(d.scale));
                    worldShader.setMat4("uModel", M);
                    for (const auto& sub : d.model->meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        sub.mesh.draw();
                    }
                }
                if (cullWas) glEnable(GL_CULL_FACE);
                checker.bind(0);
            }

            // Bats — circling in the sky over the map. Position from time +
            // per-bat orbit params; yaw aligned to the tangent direction so
            // they look like they're flying. Skeletal flap animation plays
            // per-bat with a stagger so the swarm doesn't sync.
            if (!bats.empty()) {
                GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
                glDisable(GL_CULL_FACE);
                const bool hasBones = batModel.skeleton().has_value();
                worldShader.setInt("uHasBones", hasBones ? 1 : 0);
                worldShader.setVec3("uTint", glm::vec3(1.0f));
                const float t = time.total();
                for (const auto& b : bats) {
                    const float a  = b.anglePhase + t * b.orbitSpeed;
                    const float bx = b.centerXZ.x + std::cos(a) * b.orbitRadius;
                    const float bz = b.centerXZ.y + std::sin(a) * b.orbitRadius;
                    const float by = b.altitude
                                    + std::sin(t * b.bobFreq * 6.28318530718f
                                               + b.anglePhase) * b.bobAmp;

                    // Tangent direction (derivative of cos/sin around the orbit).
                    // Sign of orbitSpeed flips the facing for CCW vs CW.
                    const float tdx = -std::sin(a) * b.orbitSpeed;
                    const float tdz =  std::cos(a) * b.orbitSpeed;
                    const float yaw = std::atan2(tdx, tdz);

                    glm::mat4 M(1.0f);
                    M = glm::translate(M, glm::vec3(bx, by, bz));
                    M = glm::rotate(M, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                    // Final orientation chain (vertex order, right to left):
                    //   X(90)  pitch the standing model into horizontal flight
                    //   Y(-90) swing the head into the chase direction
                    //   Z(180) roll 180 around the now-forward axis to flip
                    //          the bat right-side-up
                    M = glm::rotate(M, glm::radians( 180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                    M = glm::rotate(M, glm::radians( -90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    M = glm::rotate(M, glm::radians(  90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    M = glm::scale(M, glm::vec3(b.scale));
                    worldShader.setMat4("uModel", M);

                    if (hasBones) {
                        const auto& matrices = b.animator.finalBoneMatrices();
                        for (size_t i = 0; i < matrices.size() && i < 200; ++i) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "uBones[%zu]", i);
                            worldShader.setMat4(buf, matrices[i]);
                        }
                    }
                    for (const auto& sub : batModel.meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        sub.mesh.draw();
                    }
                }
                worldShader.setInt("uHasBones", 0);
                if (cullWas) glEnable(GL_CULL_FACE);
                checker.bind(0);
            }

            // Perimeter walls: 4 long thin boxes around the map edge using
            // the unit cube mesh, textured with the plastered stone diffuse.
            {
                worldShader.setInt("uHasBones", 0);
                worldShader.setVec3("uTint", glm::vec3(1.0f));  // let the texture speak
                wallTex.bind(0);
                const float L = kWallHalfMap * 2.0f;        // wall length (matches map)
                const float T = kWallHalfThickness * 2.0f;  // wall thickness
                const float H = kWallHalfHeight * 2.0f;     // wall full height
                struct WallTransform { glm::vec3 centre; glm::vec3 size; };
                const WallTransform walls[4] = {
                    { glm::vec3(0.0f, kWallY,  kWallHalfMap), glm::vec3(L, H, T) },
                    { glm::vec3(0.0f, kWallY, -kWallHalfMap), glm::vec3(L, H, T) },
                    { glm::vec3( kWallHalfMap, kWallY, 0.0f), glm::vec3(T, H, L) },
                    { glm::vec3(-kWallHalfMap, kWallY, 0.0f), glm::vec3(T, H, L) },
                };
                for (const auto& w : walls) {
                    glm::mat4 M(1.0f);
                    M = glm::translate(M, w.centre);
                    M = glm::scale(M, w.size);
                    worldShader.setMat4("uModel", M);
                    cube.draw();
                }
                checker.bind(0);  // restore default for following passes
            }

            // Enemies: track the active def to avoid redundant shader state changes
            // when consecutive enemies share a type. Cull is disabled for the
            // pass because some GLBs ship doubleSided materials or have one
            // mirrored side with inverted winding (e.g. Cat) — culling would
            // erase the back-facing half of those meshes.
            if (!enemies.empty()) {
                GLboolean enemyCullWas  = glIsEnabled(GL_CULL_FACE);
                GLboolean enemyBlendWas = glIsEnabled(GL_BLEND);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

                    // Death-fade alpha: 1.0 for the first half of the death
                    // animation, then linearly to 0 across the second half.
                    float alpha = 1.0f;
                    if (e.dying && e.deathDuration > 0.0f) {
                        const float frac = std::clamp(e.deathTimer / e.deathDuration, 0.0f, 1.0f);
                        if (frac > 0.5f) alpha = 1.0f - (frac - 0.5f) * 2.0f;
                    }
                    worldShader.setFloat("uAlpha", alpha);

                    if (activeDef->model->skeleton()) {
                        const auto& matrices = e.animator.finalBoneMatrices();
                        for (size_t i = 0; i < matrices.size() && i < 200; ++i) {
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
                worldShader.setInt  ("uHasBones", 0);
                worldShader.setFloat("uAlpha",    1.0f);  // restore default for following passes
                if (!enemyBlendWas) glDisable(GL_BLEND);
                if (enemyCullWas)   glEnable(GL_CULL_FACE);
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
                        for (size_t i = 0; i < matrices.size() && i < 200; ++i) {
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

            // Antidote boxes — natural texture with a subtle green pulse so
            // the box itself reads as faintly self-luminous (the vertical
            // beam handles long-distance visibility).
            if (!antidoteBoxes.empty()) {
                const float boxPulse = 1.05f + 0.10f * std::sin(time.total() * 2.5f);
                const glm::vec3 boxTint(0.95f * boxPulse,
                                        1.20f * boxPulse,
                                        1.00f * boxPulse);
                worldShader.setInt  ("uHasBones", 0);
                worldShader.setFloat("uAlpha",    1.0f);
                worldShader.setVec3 ("uTint",     boxTint);
                checker.bind(0);
                for (const auto& box : antidoteBoxes) {
                    if (box.state != game::AntidoteBoxState::Active) continue;
                    const float gh = terrain.heightAt(box.position.x, box.position.z);
                    glm::mat4 M(1.0f);
                    M = glm::translate(M, glm::vec3(box.position.x, gh + game::kAntidoteYOffset, box.position.z));
                    M = glm::rotate(M, box.yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
                    M = glm::rotate(M, glm::radians(game::kAntidoteBasePitchDeg),
                                    glm::vec3(1.0f, 0.0f, 0.0f));
                    M = glm::scale(M, glm::vec3(game::kAntidoteScale));
                    worldShader.setMat4("uModel", M);
                    for (const auto& sub : antidoteBoxModel.meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        else             checker.bind(0);
                        sub.mesh.draw();
                    }
                }
                worldShader.setVec3("uTint", glm::vec3(1.0f));

                // (No ground glow disc — the beacon's vertical particle
                // beam carries the visibility on its own.)
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
                    glm::vec3 vfwd;
                    if (glm::length(p.velocity) > 1e-4f) {
                        vfwd = glm::normalize(p.velocity);
                    } else if (p.sticky) {
                        vfwd = glm::vec3(0.0f, -1.0f, 0.0f);  // landed: tip points into ground
                    } else {
                        vfwd = glm::vec3(0.0f, 0.0f, -1.0f);
                    }
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
                    // Lightning shots glow bright cyan-blue while in flight;
                    // others render with the default white tint.
                    if (p.lightningDamage > 0) {
                        worldShader.setVec3("uTint", kLightningTint * 1.6f);
                    }
                    for (const auto& sub : syringeModel.meshes()) {
                        if (sub.diffuse) sub.diffuse->bind(0);
                        sub.mesh.draw();
                    }
                    if (p.lightningDamage > 0) {
                        worldShader.setVec3("uTint", glm::vec3(1.0f));
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

                // Minimap. Circular disc top-right by default; M toggles to a
                // larger centred view. Player marker is always at centre and
                // the world is rotated to put player-forward at the top.
                {
                    const int W = window.width();
                    const int H = window.height();
                    const float radius = minimapExpanded ? kMinimapExpandedPx
                                                          : kMinimapRadiusPx;
                    const float cx = minimapExpanded
                        ? W * 0.5f
                        : W - kMinimapMargin - radius;
                    const float cy = minimapExpanded
                        ? H * 0.5f
                        : kMinimapMargin + radius;

                    // Background disc + border ring.
                    minimap.drawDisc(W, H, cx, cy, radius,
                                     glm::vec4(0.04f, 0.05f, 0.06f, 0.78f),
                                     glm::vec4(0.85f, 0.85f, 0.90f, 0.85f),
                                     0.04f);

                    // Build the player-relative basis (forward + right) in
                    // the XZ plane so we can rotate world positions into
                    // map space (player-forward = up on the disc).
                    const float yawRad = glm::radians(player.camera().yaw);
                    const float fx = std::cos(yawRad);
                    const float fz = std::sin(yawRad);
                    const float rx = -fz;   // right in XZ = perpendicular to forward
                    const float rz =  fx;
                    const glm::vec3 ppos = player.position();
                    const float pixPerMetre = radius / kMinimapWorldRadius;

                    auto plotMarker = [&](const glm::vec3& worldPos,
                                          float halfSize,
                                          const glm::vec3& color,
                                          float opacity) {
                        const float dx = worldPos.x - ppos.x;
                        const float dz = worldPos.z - ppos.z;
                        const float distXZ = std::sqrt(dx * dx + dz * dz);
                        // Skip when the marker centre would be off-disc; small
                        // safety inset so the dot stays fully inside.
                        if (distXZ > kMinimapWorldRadius * 0.96f) return;
                        const float mapRight   =  dx * rx + dz * rz;
                        const float mapForward =  dx * fx + dz * fz;
                        const float px = cx + mapRight   * pixPerMetre;
                        const float py = cy - mapForward * pixPerMetre;  // forward = up = -y
                        hud.drawRect(W, H,
                                     glm::vec2(px - halfSize, py - halfSize),
                                     glm::vec2(halfSize * 2.0f, halfSize * 2.0f),
                                     color, opacity);
                    };

                    // Enemies — bright red dots, slightly bigger when expanded.
                    const float enemyDot = minimapExpanded ? 5.0f : 3.5f;
                    for (const auto& e : enemies) {
                        if (!e.alive()) continue;
                        plotMarker(e.position, enemyDot,
                                   glm::vec3(1.0f, 0.18f, 0.18f), 0.95f);
                    }

                    // Antidote boxes — bright green dot, larger so it stands out.
                    const float antidoteDot = minimapExpanded ? 8.0f : 5.5f;
                    for (const auto& b : antidoteBoxes) {
                        if (b.state != game::AntidoteBoxState::Active) continue;
                        plotMarker(b.position, antidoteDot,
                                   glm::vec3(0.30f, 1.0f, 0.45f), 0.95f);
                    }

                    // Player — white dot at the disc centre, with a thin
                    // forward indicator pointing up.
                    const float playerDot = minimapExpanded ? 7.0f : 5.0f;
                    hud.drawRect(W, H,
                                 glm::vec2(cx - playerDot, cy - playerDot),
                                 glm::vec2(playerDot * 2.0f, playerDot * 2.0f),
                                 glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);
                    // Tiny vertical bar above the dot showing facing direction.
                    const float fwBarH = playerDot * 2.4f;
                    hud.drawRect(W, H,
                                 glm::vec2(cx - 1.0f, cy - playerDot - fwBarH),
                                 glm::vec2(2.0f, fwBarH),
                                 glm::vec3(1.0f, 1.0f, 1.0f), 0.85f);
                }

                // Soft off-screen enemy indicators. Project each enemy to clip
                // space; if it's behind the camera or outside [-1,1]^2, drop a
                // small chevron at the screen edge in that direction.
                {
                    const int W = window.width();
                    const int H = window.height();
                    const glm::mat4 vp = cam.proj(window.aspect()) * cam.view();
                    const float insetX = W * 0.06f;          // edge padding
                    const float insetY = H * 0.06f;
                    const float arrowSize = 14.0f;            // pixels (half-extent of triangle)
                    const glm::vec4 arrowColor(1.0f, 0.55f, 0.45f, 0.40f);
                    for (const auto& e : enemies) {
                        if (!e.alive() || !e.def) continue;
                        glm::vec4 clip = vp * glm::vec4(e.hitCentre(), 1.0f);
                        const bool behind = clip.w <= 1e-3f;
                        glm::vec2 ndc;
                        if (behind) {
                            // Skip the perspective divide entirely. clip.xy
                            // already encodes the camera-space direction with
                            // the correct sign — dividing by negative w (or
                            // negating) would flip it the wrong way.
                            ndc = glm::vec2(clip.x, clip.y);
                        } else {
                            ndc = glm::vec2(clip.x / clip.w, clip.y / clip.w);
                        }
                        const bool onScreen = !behind &&
                            std::abs(ndc.x) <= 1.0f && std::abs(ndc.y) <= 1.0f;
                        if (onScreen) continue;
                        // Project direction onto the [-1,1] box edge (Inf-norm).
                        const float ax = std::abs(ndc.x);
                        const float ay = std::abs(ndc.y);
                        const float m  = std::max(std::max(ax, ay), 1e-3f);
                        glm::vec2 edge = ndc / m;  // now |edge|inf == 1
                        // Convert to pixel coords (top-left origin), inset from screen edge.
                        const float halfW = static_cast<float>(W) * 0.5f - insetX;
                        const float halfH = static_cast<float>(H) * 0.5f - insetY;
                        const float px = static_cast<float>(W) * 0.5f + edge.x * halfW;
                        const float py = static_cast<float>(H) * 0.5f - edge.y * halfH;
                        // Pixel-space angle: y is flipped vs NDC.
                        const float angle = std::atan2(-edge.y, edge.x);
                        arrows.draw(W, H, px, py, angle, arrowSize, arrowColor);
                    }

                    // Off-screen indicator for the antidote box (bright green arrow).
                    for (const auto& box : antidoteBoxes) {
                        if (box.state != game::AntidoteBoxState::Active) continue;
                        const glm::vec3 boxCentre(box.position.x,
                                                   terrain.heightAt(box.position.x, box.position.z) + 0.5f,
                                                   box.position.z);
                        glm::vec4 clip = vp * glm::vec4(boxCentre, 1.0f);
                        const bool behind = clip.w <= 1e-3f;
                        glm::vec2 ndc;
                        if (behind) {
                            ndc = glm::vec2(clip.x, clip.y);
                        } else {
                            ndc = glm::vec2(clip.x / clip.w, clip.y / clip.w);
                        }
                        const bool onScreen = !behind &&
                            std::abs(ndc.x) <= 1.0f && std::abs(ndc.y) <= 1.0f;
                        if (!onScreen) {
                            const float axb = std::abs(ndc.x);
                            const float ayb = std::abs(ndc.y);
                            const float mb  = std::max(std::max(axb, ayb), 1e-3f);
                            glm::vec2 edgeB = ndc / mb;
                            const float halfWb = static_cast<float>(W) * 0.5f - insetX;
                            const float halfHb = static_cast<float>(H) * 0.5f - insetY;
                            const float pxb = static_cast<float>(W) * 0.5f + edgeB.x * halfWb;
                            const float pyb = static_cast<float>(H) * 0.5f - edgeB.y * halfHb;
                            const float angb = std::atan2(-edgeB.y, edgeB.x);
                            arrows.draw(W, H, pxb, pyb, angb, 18.0f,
                                        glm::vec4(0.3f, 1.0f, 0.5f, 0.85f));
                        }
                    }
                }

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
                        std::snprintf(waveBuf, sizeof(waveBuf),
                                      "WAVE %d / %d  %s  %.0fs  HPx%.1f",
                                      waveManager.currentWaveIndex() + 1,
                                      waveManager.totalWaves(),
                                      waveManager.currentWaveName().c_str(),
                                      waveManager.waveTimeRemaining(),
                                      spawner.hpMultiplier());
                    }
                    const std::string ws = waveBuf;
                    const float wScale = 1.6f;
                    const float wW     = render::Text::measure(ws) * wScale;
                    const float waveLineY = hud.xpBar().topPx + hud.xpBar().sizePx.y + 14.0f;
                    text.draw(window.width(), window.height(),
                              window.width() * 0.5f - wW * 0.5f,
                              waveLineY,
                              ws, wScale,
                              glm::vec4(1.0f, 0.92f, 0.78f, 1.0f));

                    // Antidote status line: distance while active, pulsing warning
                    // when the wave timer expired but the box is not yet collected.
                    if (!antidoteBoxes.empty() && !waveManager.finished()) {
                        const auto& box = antidoteBoxes.front();
                        const float dx  = box.position.x - player.position().x;
                        const float dz  = box.position.z - player.position().z;
                        const float boxDist = std::sqrt(dx * dx + dz * dz);
                        char antiBuf[64];
                        std::snprintf(antiBuf, sizeof(antiBuf),
                                      "COLLECT ANTIDOTE BOX  %.0fm", boxDist);
                        const std::string antiStr = antiBuf;
                        const float antiScale = 1.6f;
                        const float antiW = render::Text::measure(antiStr) * antiScale;
                        const float pulse = 0.65f + 0.35f * std::sin(
                            waveManager.waveElapsed() * 5.0f);
                        const float urgency = waveManager.waitingForAntidote() ? 1.0f : 0.55f;
                        text.draw(window.width(), window.height(),
                                  window.width() * 0.5f - antiW * 0.5f,
                                  waveLineY + wScale * 10.0f + 6.0f,
                                  antiStr, antiScale,
                                  glm::vec4(0.35f, 1.0f, 0.5f, pulse * urgency));
                    }
                }
                journalScreen.drawToast(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::Settings) {
                settingsMenu.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::LevelUp) {
                levelUpMenu.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::GameOver) {
                const int W = window.width();
                const int H = window.height();

                // Full-screen dark overlay.
                hud.drawRect(W, H, glm::vec2(0, 0),
                             glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                             glm::vec3(0.04f, 0.0f, 0.0f), 0.82f);

                // Panel.
                const float panelW = 520.0f;
                const float panelH = 240.0f;
                const float panelX = W * 0.5f - panelW * 0.5f;
                const float panelY = H * 0.5f - panelH * 0.5f;
                const glm::vec3 panelCol { 0.08f, 0.02f, 0.02f };
                const glm::vec3 borderCol { 0.75f, 0.12f, 0.10f };
                hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                                 glm::vec2(panelW, panelH),
                                 1.0f, panelCol, panelCol, borderCol, 2.5f, 0.96f);

                {
                    const std::string title = "GAME OVER";
                    const float sc = 5.0f;
                    const float w  = render::Text::measure(title) * sc;
                    text.draw(W, H, panelX + (panelW - w) * 0.5f,
                              panelY + 36.0f, title, sc,
                              glm::vec4(0.95f, 0.18f, 0.15f, 1.0f));
                }
                {
                    const std::string wave = "Wave " +
                        std::to_string(waveManager.currentWaveIndex() + 1) +
                        " of " + std::to_string(waveManager.totalWaves());
                    const float sc = 1.8f;
                    const float w  = render::Text::measure(wave) * sc;
                    text.draw(W, H, panelX + (panelW - w) * 0.5f,
                              panelY + 130.0f, wave, sc,
                              glm::vec4(0.75f, 0.60f, 0.58f, 1.0f));
                }
                {
                    const std::string hint = "PRESS R TO RESTART";
                    const float sc = 2.0f;
                    const float w  = render::Text::measure(hint) * sc;
                    text.draw(W, H, panelX + (panelW - w) * 0.5f,
                              panelY + panelH - 44.0f, hint, sc,
                              glm::vec4(0.80f, 0.70f, 0.68f, 1.0f));
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
                ps.hailCount    = player.countItem(game::ItemId::HailRing);
                ps.explosiveAutoCount = player.countItem(game::ItemId::ExplosiveAuto);
                ps.lightningCount = player.countItem(game::ItemId::LightningRing);
                ps.items        = player.inventory();
                statsScreen.draw(hud, text, window.width(), window.height(), ps);
            } else if (scene == game::Scene::Journal) {
                journalScreen.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::StartMenu) {
                startMenu.draw(hud, text, window.width(), window.height());
            } else if (scene == game::Scene::Victory) {
                const int   W      = window.width();
                const int   H      = window.height();
                const float panelW = 640.0f;
                const float panelH = 340.0f;
                const float panelX = W * 0.5f - panelW * 0.5f;
                const float panelY = H * 0.5f - panelH * 0.5f;

                hud.drawRect(W, H, glm::vec2(0.0f, 0.0f),
                             glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                             glm::vec3(0.0f), 0.80f);
                hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                                 glm::vec2(panelW, panelH),
                                 1.0f,
                                 glm::vec3(0.02f, 0.06f, 0.02f),
                                 glm::vec3(0.02f, 0.06f, 0.02f),
                                 glm::vec3(0.20f, 0.65f, 0.20f),
                                 2.5f, 0.97f);
                {
                    const std::string title = "MISSION COMPLETE";
                    const float sc = 4.0f;
                    const float tw = render::Text::measure(title) * sc;
                    text.draw(W, H, panelX + (panelW - tw) * 0.5f,
                              panelY + 36.0f, title, sc,
                              glm::vec4(0.40f, 1.00f, 0.45f, 1.0f));
                }
                {
                    const std::string sub = "All antidote caches secured. Zone Alpha-7 contained.";
                    const float sc = 1.7f;
                    const float sw = render::Text::measure(sub) * sc;
                    text.draw(W, H, panelX + (panelW - sw) * 0.5f,
                              panelY + 130.0f, sub, sc,
                              glm::vec4(0.75f, 0.92f, 0.75f, 1.0f));
                }
                {
                    const std::string waves = "All " +
                        std::to_string(waveManager.totalWaves()) + " waves survived.";
                    const float sc = 1.8f;
                    const float ww = render::Text::measure(waves) * sc;
                    text.draw(W, H, panelX + (panelW - ww) * 0.5f,
                              panelY + 170.0f, waves, sc,
                              glm::vec4(0.60f, 0.88f, 0.62f, 1.0f));
                }
                {
                    const std::string lvl = "Reached level " + std::to_string(player.level()) + ".";
                    const float sc = 1.8f;
                    const float lw = render::Text::measure(lvl) * sc;
                    text.draw(W, H, panelX + (panelW - lw) * 0.5f,
                              panelY + 206.0f, lvl, sc,
                              glm::vec4(0.60f, 0.88f, 0.62f, 1.0f));
                }
                {
                    const std::string hint = "PRESS R TO PLAY AGAIN";
                    const float sc = 2.0f;
                    const float hw = render::Text::measure(hint) * sc;
                    text.draw(W, H, panelX + (panelW - hw) * 0.5f,
                              panelY + panelH - 44.0f, hint, sc,
                              glm::vec4(0.70f, 0.90f, 0.70f, 1.0f));
                }
            }

            // Loot toasts — top-left achievement style. Slide in, hold, fade.
            // Drawn over any scene so the player sees pickups even mid-menu.
            if (!lootToasts.empty()) {
                const int W = window.width();
                const int H = window.height();
                const float toastW   = 360.0f;
                const float toastH   = 64.0f;
                const float gap      = 8.0f;
                const float marginX  = 24.0f;
                const float marginY  = 24.0f;
                for (size_t i = 0; i < lootToasts.size(); ++i) {
                    const LootToast& t = lootToasts[i];
                    // Slide in from off-screen left for the first kToastSlideIn
                    // seconds, then hold, then fade alpha to zero in the tail.
                    float slide = std::clamp(t.age / kToastSlideIn, 0.0f, 1.0f);
                    // Smooth ease-out for the entry.
                    slide = 1.0f - (1.0f - slide) * (1.0f - slide);
                    const float fade = (t.age > kToastLifetime - kToastFadeOut)
                        ? std::clamp((kToastLifetime - t.age) / kToastFadeOut, 0.0f, 1.0f)
                        : 1.0f;
                    const float x = -toastW + (marginX + toastW) * slide;
                    const float y = marginY + static_cast<float>(i) * (toastH + gap);
                    const glm::vec3 rcol = game::rarityColor(t.item.rarity);
                    const glm::vec3 panelCol(0.05f, 0.04f, 0.04f);
                    const float opacity = 0.92f * fade;

                    hud.drawProgress(W, H, glm::vec2(x, y),
                                     glm::vec2(toastW, toastH),
                                     1.0f, panelCol, panelCol, rcol, 2.0f, opacity);

                    // Rarity-coloured accent stripe on the left edge.
                    hud.drawRect(W, H, glm::vec2(x, y),
                                 glm::vec2(5.0f, toastH), rcol, opacity);

                    // "OBTAINED" small label, then item name in rarity colour.
                    const float textX = x + 16.0f;
                    text.draw(W, H, textX, y + 12.0f, "OBTAINED", 1.1f,
                              glm::vec4(0.78f, 0.74f, 0.72f, opacity));
                    text.draw(W, H, textX, y + 30.0f,
                              game::itemName(t.item.id), 2.1f,
                              glm::vec4(rcol, opacity));
                    // Rarity name on the right.
                    const std::string rname = game::rarityName(t.item.rarity);
                    const float rscale = 1.1f;
                    const float rw = render::Text::measure(rname) * rscale;
                    text.draw(W, H, x + toastW - rw - 14.0f, y + toastH - 16.0f,
                              rname, rscale, glm::vec4(rcol, opacity * 0.85f));
                }
            }

            // Software cursor — drawn on top of any menu so the player can
            // see what they're clicking even in fullscreen (where the OS
            // cursor is unreliable). Only the menus need it; gameplay is
            // FPS-locked and shouldn't show one.
            if (scene != game::Scene::Playing) {
                const float mx   = static_cast<float>(input.mouseX());
                const float my   = static_cast<float>(input.mouseY());
                const float half = 6.0f;
                hud.drawProgress(window.width(), window.height(),
                                 glm::vec2(mx - half, my - half),
                                 glm::vec2(half * 2.0f, half * 2.0f),
                                 1.0f,
                                 glm::vec3(0.96f, 0.96f, 0.96f),
                                 glm::vec3(0.96f, 0.96f, 0.96f),
                                 glm::vec3(0.05f, 0.05f, 0.05f),
                                 1.5f, 0.95f);
            }

            window.swap();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}



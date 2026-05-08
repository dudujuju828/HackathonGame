#include "core/Input.h"
#include "core/Time.h"
#include "core/Window.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "render/Shader.h"
#include "render/Texture.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
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
        verts.push_back({f.a, f.n, {0, 0}});
        verts.push_back({f.b, f.n, {1, 0}});
        verts.push_back({f.c, f.n, {1, 1}});
        verts.push_back({f.d, f.n, {0, 1}});
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

        render::Camera cam;
        cam.position = {0.0f, 1.6f, 3.0f};

        const float moveSpeed = 4.0f;

        glClearColor(0.02f, 0.02f, 0.03f, 1.0f);

        while (!window.shouldClose()) {
            window.pollEvents();
            input.update();
            time.tick();
            float dt = time.dt();

            if (input.key(GLFW_KEY_ESCAPE)) break;

            cam.addLook(input.mouseDX(), input.mouseDY());

            glm::vec3 fwd = cam.forward();
            fwd.y = 0.0f;
            if (glm::length(fwd) > 0.0001f) fwd = glm::normalize(fwd);
            glm::vec3 rt = glm::normalize(glm::cross(fwd, glm::vec3(0,1,0)));

            glm::vec3 wish(0.0f);
            if (input.key(GLFW_KEY_W)) wish += fwd;
            if (input.key(GLFW_KEY_S)) wish -= fwd;
            if (input.key(GLFW_KEY_D)) wish += rt;
            if (input.key(GLFW_KEY_A)) wish -= rt;
            if (glm::length(wish) > 0.0001f) {
                wish = glm::normalize(wish) * moveSpeed * dt;
                cam.position += wish;
            }

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            worldShader.use();
            checker.bind(0);
            worldShader.setInt  ("uAlbedo", 0);
            worldShader.setMat4 ("uViewProj", cam.proj(window.aspect()) * cam.view());
            worldShader.setVec3 ("uCamPos", cam.position);
            worldShader.setVec3 ("uCamDir", cam.forward());
            worldShader.setVec3 ("uAmbient", glm::vec3(0.04f));
            worldShader.setFloat("uFlashInner", glm::cos(glm::radians(12.0f)));
            worldShader.setFloat("uFlashOuter", glm::cos(glm::radians(20.0f)));
            worldShader.setVec3 ("uFlashColor", glm::vec3(1.2f, 1.15f, 1.0f));

            // A small grid of cubes to wander around.
            for (int z = -3; z <= 3; ++z) {
                for (int x = -3; x <= 3; ++x) {
                    if ((x + z) & 1) continue;
                    glm::mat4 M(1.0f);
                    M = glm::translate(M, glm::vec3(x * 2.0f, 0.5f, z * 2.0f));
                    worldShader.setMat4("uModel", M);
                    cube.draw();
                }
            }

            // Floor: a flat scaled cube.
            {
                glm::mat4 M(1.0f);
                M = glm::translate(M, glm::vec3(0.0f, -0.05f, 0.0f));
                M = glm::scale(M, glm::vec3(40.0f, 0.1f, 40.0f));
                worldShader.setMat4("uModel", M);
                cube.draw();
            }

            window.swap();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}

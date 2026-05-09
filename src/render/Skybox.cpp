#include "Skybox.h"

#include <glad/gl.h>
#include <stb_image.h>

#include <cstdio>

namespace render {

Skybox::~Skybox() { release(); }

void Skybox::release() {
    if (vbo_)    glDeleteBuffers(1, &vbo_);
    if (vao_)    glDeleteVertexArrays(1, &vao_);
    if (hdrTex_) glDeleteTextures(1, &hdrTex_);
    vao_ = vbo_ = 0;
    hdrTex_ = 0;
}

bool Skybox::init(const std::string& vertPath,
                  const std::string& fragPath,
                  const std::string& hdrPath) {
    if (!shader_.loadFromFiles(vertPath, fragPath)) return false;

    // Two-tri fullscreen quad in NDC.
    const float verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    glBindVertexArray(0);

    // HDR equirectangular: stbi_loadf gives float radiance. Don't flip
    // vertically — typical equirectangular HDRs already have +Y as the
    // up direction in their pixel layout.
    int w = 0, h = 0, c = 0;
    stbi_set_flip_vertically_on_load(false);
    float* data = stbi_loadf(hdrPath.c_str(), &w, &h, &c, 3);
    // Restore the OBJ/textures default for downstream loads.
    stbi_set_flip_vertically_on_load(true);
    if (!data) {
        std::fprintf(stderr, "[skybox] failed to load HDR '%s'\n", hdrPath.c_str());
        return false;
    }

    glGenTextures(1, &hdrTex_);
    glBindTexture(GL_TEXTURE_2D, hdrTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);

    std::printf("[skybox] loaded HDR '%s' (%dx%d)\n", hdrPath.c_str(), w, h);
    return true;
}

void Skybox::draw(const glm::mat4& invViewProj,
                  const glm::vec3& cameraPos,
                  float exposure) {
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean blendWas = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    shader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex_);
    shader_.setInt  ("uSky",         0);
    shader_.setMat4 ("uInvViewProj", invViewProj);
    shader_.setVec3 ("uCameraPos",   cameraPos);
    shader_.setFloat("uExposure",    exposure);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    if (depthWas) glEnable(GL_DEPTH_TEST);
    if (cullWas)  glEnable(GL_CULL_FACE);
    if (blendWas) glEnable(GL_BLEND);
}

}  // namespace render

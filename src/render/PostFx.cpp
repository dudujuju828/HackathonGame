#include "PostFx.h"

#include <glad/gl.h>

namespace render {

PostFx::~PostFx() { release(); }

void PostFx::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = 0;
}

bool PostFx::init(const std::string& vertPath, const std::string& fragPath) {
    if (!shader_.loadFromFiles(vertPath, fragPath)) return false;

    // Two-tri quad covering NDC [-1, 1].
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
    return true;
}

void PostFx::apply(unsigned int sceneTexture,
                   int   targetW,
                   int   targetH,
                   float timeSeconds,
                   const CrtParams& p) {
    // Post pass: no depth test, no culling.
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    shader_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    shader_.setInt  ("uScene", 0);
    shader_.setVec3 ("uResolution",
        glm::vec3(static_cast<float>(targetW),
                  static_cast<float>(targetH),
                  0.0f));
    shader_.setFloat("uTime",             timeSeconds);
    shader_.setFloat("uCurvature",        p.curvature);
    shader_.setFloat("uScanlineStrength", p.scanlineStrength);
    shader_.setFloat("uScanlineCount",    p.scanlineCount);
    shader_.setFloat("uAberrationPx",     p.aberrationPx);
    shader_.setFloat("uVignette",         p.vignette);
    shader_.setFloat("uNoise",            p.noise);
    shader_.setFloat("uFlicker",          p.flicker);
    shader_.setFloat("uMaskStrength",     p.maskStrength);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    if (depthWas) glEnable(GL_DEPTH_TEST);
    if (cullWas)  glEnable(GL_CULL_FACE);
}

}  // namespace render

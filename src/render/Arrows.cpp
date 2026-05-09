#include "Arrows.h"

#include <glad/gl.h>

namespace render {

Arrows::~Arrows() { release(); }

void Arrows::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = 0;
}

bool Arrows::init(const std::string& vertPath, const std::string& fragPath) {
    if (!shader_.loadFromFiles(vertPath, fragPath)) return false;

    // Triangle pointing +X (tip at (1, 0), base centred on origin).
    // Stubby chevron-ish shape — feels softer than a sharp dart.
    const float verts[] = {
         1.0f,  0.0f,
        -0.7f,  0.55f,
        -0.7f, -0.55f,
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

void Arrows::draw(int targetW, int targetH,
                  float centerXPx, float centerYPx,
                  float angleRad, float sizePx,
                  const glm::vec4& color) {
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean blendWas = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    shader_.setVec3 ("uResolution", glm::vec3(static_cast<float>(targetW),
                                              static_cast<float>(targetH), 0.0f));
    shader_.setVec3 ("uCenter",     glm::vec3(centerXPx, centerYPx, 0.0f));
    shader_.setFloat("uAngle",      angleRad);
    shader_.setFloat("uSize",       sizePx);
    shader_.setVec4 ("uColor",      color);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    if (!blendWas) glDisable(GL_BLEND);
    if (cullWas)   glEnable(GL_CULL_FACE);
    if (depthWas)  glEnable(GL_DEPTH_TEST);
}

}  // namespace render

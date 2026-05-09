#include "Glow.h"

#include <glad/gl.h>

namespace render {

Glow::~Glow() { release(); }

void Glow::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = 0;
}

bool Glow::init(const std::string& vertPath, const std::string& fragPath) {
    if (!shader_.loadFromFiles(vertPath, fragPath)) return false;

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

void Glow::draw(const glm::mat4& viewProj,
                const glm::vec3& center,
                float radius,
                const glm::vec3& color,
                float intensity) {
    GLboolean blendWas = glIsEnabled(GL_BLEND);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);  // don't write depth so subsequent transparent passes work

    shader_.use();
    shader_.setMat4 ("uViewProj",  viewProj);
    shader_.setVec3 ("uCenter",    center);
    shader_.setFloat("uRadius",    radius);
    shader_.setVec3 ("uColor",     color);
    shader_.setFloat("uIntensity", intensity);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    if (!depthWas) glDisable(GL_DEPTH_TEST);
    if (cullWas)   glEnable(GL_CULL_FACE);
    if (!blendWas) glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // restore default
}

}  // namespace render

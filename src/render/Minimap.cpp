#include "Minimap.h"

#include <glad/gl.h>

namespace render {

Minimap::~Minimap() { release(); }

void Minimap::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = 0;
}

bool Minimap::init(const std::string& vertPath, const std::string& fragPath) {
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

void Minimap::drawDisc(int targetW, int targetH,
                       float centerXPx, float centerYPx,
                       float radiusPx,
                       const glm::vec4& fillColor,
                       const glm::vec4& borderColor,
                       float borderFrac) {
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
    shader_.setFloat("uRadius",     radiusPx);
    shader_.setVec4 ("uFillColor",  fillColor);
    shader_.setVec4 ("uBorderColor", borderColor);
    shader_.setFloat("uBorderFrac", borderFrac);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    if (!blendWas) glDisable(GL_BLEND);
    if (cullWas)   glEnable(GL_CULL_FACE);
    if (depthWas)  glEnable(GL_DEPTH_TEST);
}

}  // namespace render

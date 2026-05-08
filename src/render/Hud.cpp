#include "Hud.h"

#include <glad/gl.h>

namespace render {

Hud::~Hud() { release(); }

void Hud::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = 0;
}

bool Hud::init(const std::string& crosshairVert,
               const std::string& crosshairFrag) {
    if (!crosshairShader_.loadFromFiles(crosshairVert, crosshairFrag)) return false;

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

void Hud::drawCrosshair(int targetW, int targetH, const CrosshairParams& p) {
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean blendWas = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    crosshairShader_.use();
    crosshairShader_.setVec3 ("uResolution",
        glm::vec3(static_cast<float>(targetW),
                  static_cast<float>(targetH), 0.0f));
    crosshairShader_.setVec3 ("uColor",     p.color);
    crosshairShader_.setFloat("uSize",      p.size);
    crosshairShader_.setFloat("uThickness", p.thickness);
    crosshairShader_.setFloat("uGap",       p.gap);
    crosshairShader_.setFloat("uOutline",   p.outline);
    crosshairShader_.setFloat("uOpacity",   p.opacity);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    if (!blendWas) glDisable(GL_BLEND);
    if (depthWas)  glEnable(GL_DEPTH_TEST);
    if (cullWas)   glEnable(GL_CULL_FACE);
}

}  // namespace render

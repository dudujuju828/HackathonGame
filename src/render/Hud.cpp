#include "Hud.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>

namespace render {

Hud::~Hud() { release(); }

void Hud::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = 0;
}

bool Hud::init(const std::string& shaderDir) {
    if (!crosshairShader_.loadFromFiles(shaderDir + "/hud_crosshair.vert",
                                        shaderDir + "/hud_crosshair.frag")) return false;
    if (!barShader_.loadFromFiles(shaderDir + "/hud_bar.vert",
                                  shaderDir + "/hud_bar.frag")) return false;

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

namespace {
struct GLStateGuard {
    GLboolean depth, cull, blend;
    GLStateGuard() {
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull  = glIsEnabled(GL_CULL_FACE);
        blend = glIsEnabled(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    ~GLStateGuard() {
        if (!blend) glDisable(GL_BLEND);
        if (depth)  glEnable(GL_DEPTH_TEST);
        if (cull)   glEnable(GL_CULL_FACE);
    }
};
}  // namespace

void Hud::drawCrosshair(int targetW, int targetH, const CrosshairParams& p) {
    GLStateGuard guard;

    crosshairShader_.use();
    crosshairShader_.setVec3 ("uResolution",
        glm::vec3(static_cast<float>(targetW), static_cast<float>(targetH), 0.0f));
    crosshairShader_.setVec3 ("uColor",     p.color);
    crosshairShader_.setFloat("uSize",      p.size);
    crosshairShader_.setFloat("uThickness", p.thickness);
    crosshairShader_.setFloat("uGap",       p.gap);
    crosshairShader_.setFloat("uOutline",   p.outline);
    crosshairShader_.setFloat("uOpacity",   p.opacity);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Hud::drawBar_(int targetW, int targetH,
                   const glm::vec2& origin, const glm::vec2& sizePx,
                   float fill, int segments,
                   const glm::vec3& fillColor,
                   const glm::vec3& emptyColor,
                   const glm::vec3& borderColor,
                   float borderPx,
                   float pulse,
                   float opacity) {
    GLStateGuard guard;

    barShader_.use();
    barShader_.setVec3 ("uResolution",
        glm::vec3(static_cast<float>(targetW), static_cast<float>(targetH), 0.0f));

    // glm::vec2 via setVec3 with z=0 — Shader only exposes vec3.
    barShader_.setVec3("uOrigin", glm::vec3(origin.x, origin.y, 0.0f));
    barShader_.setVec3("uSize",   glm::vec3(sizePx.x, sizePx.y, 0.0f));

    barShader_.setFloat("uFill",        fill);
    barShader_.setInt  ("uSegments",    segments);
    barShader_.setVec3 ("uFillColor",   fillColor);
    barShader_.setVec3 ("uEmptyColor",  emptyColor);
    barShader_.setVec3 ("uBorderColor", borderColor);
    barShader_.setFloat("uBorderPx",    borderPx);
    barShader_.setFloat("uPulse",       pulse);
    barShader_.setFloat("uOpacity",     opacity);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Hud::drawHealthBar(int targetW, int targetH, float timeSeconds,
                        const HealthBarParams& p) {
    const float frac = std::clamp(p.fraction, 0.0f, 1.0f);

    // 0 when healthy, 1 when at zero — drives both color blend and pulse depth.
    const float critness = 1.0f -
        std::clamp(frac / std::max(p.criticalThreshold, 0.001f), 0.0f, 1.0f);

    glm::vec3 fillColor = glm::mix(p.colorFull, p.colorCritical, critness);
    float pulse = (std::sin(timeSeconds * 8.0f) * 0.5f + 0.5f) * critness * 0.5f;

    glm::vec2 origin { p.marginPx.x, p.marginPx.y };
    drawBar_(targetW, targetH, origin, p.sizePx, frac, /*segments=*/0,
             fillColor, p.emptyColor, p.borderColor, p.borderPx, pulse, p.opacity);
}

void Hud::drawAmmo(int targetW, int targetH, const AmmoParams& p) {
    const int   segments = std::max(p.capacity, 1);
    const int   current  = std::clamp(p.current, 0, segments);
    const float frac     = static_cast<float>(current) / static_cast<float>(segments);

    glm::vec2 origin {
        static_cast<float>(targetW) - p.marginPx.x - p.sizePx.x,
        p.marginPx.y
    };
    drawBar_(targetW, targetH, origin, p.sizePx, frac, segments,
             p.fillColor, p.emptyColor, p.borderColor, p.borderPx, 0.0f, p.opacity);
}

}  // namespace render

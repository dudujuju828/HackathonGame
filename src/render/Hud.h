#pragma once

#include "Shader.h"

#include <glm/glm.hpp>
#include <string>

namespace render {

struct CrosshairParams {
    glm::vec3 color     { 1.0f, 1.0f, 1.0f };
    float     size      = 9.0f;   // arm length from center, px
    float     thickness = 2.0f;   // arm full thickness, px
    float     gap       = 4.0f;   // center gap radius, px
    float     outline   = 1.0f;   // dark outline width, px (0 to disable)
    float     opacity   = 0.9f;
};

struct HealthBarParams {
    float     fraction          = 1.0f;          // 0..1
    glm::vec2 sizePx            { 220.0f, 14.0f };
    glm::vec2 marginPx          { 24.0f, 24.0f }; // from bottom-left
    glm::vec3 colorFull         { 0.78f, 0.10f, 0.10f };
    glm::vec3 colorCritical     { 1.00f, 0.20f, 0.10f };
    glm::vec3 emptyColor        { 0.05f, 0.02f, 0.02f };
    glm::vec3 borderColor       { 0.55f, 0.40f, 0.40f };
    float     borderPx          = 1.0f;
    float     criticalThreshold = 0.30f;          // pulse below this
    float     opacity           = 0.92f;
};

// Player-progression bar (top-centre, thinner than health for visual hierarchy).
struct XpBarParams {
    int       level    = 1;
    int       xp       = 0;
    int       xpToNext = 100;
    glm::vec2 sizePx       { 360.0f, 10.0f };
    float     topPx       = 24.0f;            // distance from top of screen
    glm::vec3 fillColor   { 0.45f, 0.85f, 0.55f };
    glm::vec3 emptyColor  { 0.04f, 0.08f, 0.05f };
    glm::vec3 borderColor { 0.35f, 0.55f, 0.40f };
    float     borderPx    = 1.0f;
    float     opacity     = 0.92f;
};

// Ammo currently in the magazine. The bar segments to match capacity so the
// HUD reads the same regardless of weapon (pistol cap=12, shotgun cap=8, etc.).
struct AmmoParams {
    int       current     = 12;
    int       capacity    = 12;
    glm::vec2 sizePx      { 220.0f, 14.0f };
    glm::vec2 marginPx    { 24.0f, 24.0f }; // from bottom-right
    glm::vec3 fillColor   { 0.95f, 0.85f, 0.40f };
    glm::vec3 emptyColor  { 0.06f, 0.05f, 0.04f };
    glm::vec3 borderColor { 0.55f, 0.50f, 0.40f };
    float     borderPx    = 1.0f;
    float     opacity     = 0.92f;
};

class Hud {
public:
    Hud() = default;
    ~Hud();

    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    // Loads hud_crosshair.{vert,frag} and hud_bar.{vert,frag} from shaderDir.
    bool init(const std::string& shaderDir = "assets/shaders");

    // Each draw expects its target framebuffer + viewport already bound.
    void drawCrosshair (int targetW, int targetH, const CrosshairParams& p);
    void drawHealthBar (int targetW, int targetH, float timeSeconds,
                        const HealthBarParams& p);
    void drawAmmo      (int targetW, int targetH, const AmmoParams& p);
    void drawXpBar     (int targetW, int targetH, const XpBarParams& p);

    // General-purpose bar primitive (used by health/ammo internally; also
    // exposed for menu sliders, panels, etc.).
    void drawRect      (int targetW, int targetH,
                        const glm::vec2& origin, const glm::vec2& size,
                        const glm::vec3& color, float opacity);
    void drawProgress  (int targetW, int targetH,
                        const glm::vec2& origin, const glm::vec2& size,
                        float fill,
                        const glm::vec3& fillColor,
                        const glm::vec3& emptyColor,
                        const glm::vec3& borderColor,
                        float borderPx,
                        float opacity);

    CrosshairParams& crosshair() { return crosshair_; }
    HealthBarParams& healthBar() { return healthBar_; }
    AmmoParams&      ammo()      { return ammo_; }
    XpBarParams&     xpBar()     { return xpBar_; }

private:
    void release();
    void drawBar_(int targetW, int targetH,
                  const glm::vec2& origin, const glm::vec2& sizePx,
                  float fill, int segments,
                  const glm::vec3& fillColor,
                  const glm::vec3& emptyColor,
                  const glm::vec3& borderColor,
                  float borderPx,
                  float pulse,
                  float opacity);

    Shader        crosshairShader_;
    Shader        barShader_;
    unsigned int  vao_ = 0;
    unsigned int  vbo_ = 0;

    CrosshairParams crosshair_ {};
    HealthBarParams healthBar_ {};
    AmmoParams      ammo_ {};
    XpBarParams     xpBar_ {};
};

}  // namespace render

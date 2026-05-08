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
    float     opacity   = 0.9f;   // 0..1
};

class Hud {
public:
    Hud() = default;
    ~Hud();

    Hud(const Hud&) = delete;
    Hud& operator=(const Hud&) = delete;

    bool init(const std::string& crosshairVert,
              const std::string& crosshairFrag);

    // Caller is responsible for binding the target framebuffer + viewport.
    void drawCrosshair(int targetW, int targetH, const CrosshairParams& p);

    CrosshairParams& crosshair() { return crosshair_; }

private:
    void release();

    Shader        crosshairShader_;
    unsigned int  vao_ = 0;
    unsigned int  vbo_ = 0;
    CrosshairParams crosshair_ {};
};

}  // namespace render

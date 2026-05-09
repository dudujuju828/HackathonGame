#pragma once

#include "Shader.h"

#include <glm/glm.hpp>

#include <string>

namespace render {

// Tiny helper for drawing rotated screen-space triangles — used as a soft
// off-screen enemy indicator. One draw call per arrow; cheap at the
// scale we use it.
class Arrows {
public:
    Arrows() = default;
    ~Arrows();
    Arrows(const Arrows&) = delete;
    Arrows& operator=(const Arrows&) = delete;

    bool init(const std::string& vertPath, const std::string& fragPath);

    // Caller binds the target FBO. Center is pixel-space, top-left origin.
    // Angle is radians (0 = pointing +X, increasing clockwise in pixel space).
    void draw(int targetW, int targetH,
              float centerXPx, float centerYPx,
              float angleRad, float sizePx,
              const glm::vec4& color);

private:
    void release();
    Shader shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};

}  // namespace render

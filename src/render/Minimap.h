#pragma once

#include "Shader.h"

#include <glm/glm.hpp>

#include <string>

namespace render {

// Tiny circular HUD primitive: a soft-edged filled disc with an optional
// border ring. Used as the minimap backdrop; markers are layered on top
// with the existing Hud::drawRect helper.
class Minimap {
public:
    Minimap() = default;
    ~Minimap();
    Minimap(const Minimap&) = delete;
    Minimap& operator=(const Minimap&) = delete;

    bool init(const std::string& vertPath, const std::string& fragPath);

    // Draws the disc at the given pixel-space centre + radius. fillColor is
    // the inner area; borderColor is the ring. borderFrac (0..1) is the
    // fraction of the radius dedicated to that ring (e.g. 0.06 for a thin
    // ring on a 100 px radius disc).
    void drawDisc(int targetW, int targetH,
                  float centerXPx, float centerYPx,
                  float radiusPx,
                  const glm::vec4& fillColor,
                  const glm::vec4& borderColor,
                  float borderFrac);

private:
    void release();
    Shader shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};

}  // namespace render

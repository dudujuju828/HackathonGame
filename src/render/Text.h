#pragma once

#include "Shader.h"

#include <glm/glm.hpp>
#include <string>

namespace render {

// Pixel-aligned ASCII text via stb_easy_font. Sufficient for menus / debug HUD;
// not pretty, no kerning, no Unicode.
class Text {
public:
    Text() = default;
    ~Text();

    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;

    bool init(const std::string& vertPath, const std::string& fragPath);

    // Draws `s` at (x, y) in window pixels (top-left origin), sized by `scale`
    // (1.0 = native ~7-px-tall stb_easy_font). Caller binds the target FBO.
    void draw(int targetW, int targetH,
              float x, float y,
              const std::string& s,
              float scale,
              const glm::vec4& color);

    // Width of `s` in pixels at scale 1.
    static float measure(const std::string& s);
    static float lineHeightPx() { return 12.0f; }

private:
    void release();

    Shader        shader_;
    unsigned int  vao_ = 0;
    unsigned int  vbo_ = 0;
    unsigned int  ebo_ = 0;
};

}  // namespace render

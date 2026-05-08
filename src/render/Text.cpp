#include "Text.h"

#include <glad/gl.h>
#include <stb_easy_font.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace render {

Text::~Text() { release(); }

void Text::release() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = ebo_ = 0;
}

bool Text::init(const std::string& vertPath, const std::string& fragPath) {
    if (!shader_.loadFromFiles(vertPath, fragPath)) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    glBindVertexArray(0);
    return true;
}

float Text::measure(const std::string& s) {
    return static_cast<float>(stb_easy_font_width(const_cast<char*>(s.c_str())));
}

void Text::draw(int targetW, int targetH,
                float x, float y,
                const std::string& s,
                float scale,
                const glm::vec4& color) {
    if (s.empty()) return;

    // stb_easy_font emits 16 bytes per vertex (vec3 + uchar[4] colour),
    // 4 verts per quad. We re-emit positions only; colour is a uniform.
    static constexpr int kVertexBytes = 16;

    static char scratch[1 << 16];  // ~50 KB; ample for any line we'd draw
    int numQuads = stb_easy_font_print(0.0f, 0.0f,
                                       const_cast<char*>(s.c_str()),
                                       nullptr, scratch, sizeof(scratch));
    if (numQuads <= 0) return;

    std::vector<float>    verts;
    std::vector<uint32_t> indices;
    verts.reserve(static_cast<size_t>(numQuads) * 4 * 2);
    indices.reserve(static_cast<size_t>(numQuads) * 6);

    for (int q = 0; q < numQuads; ++q) {
        for (int i = 0; i < 4; ++i) {
            const char* vp = scratch + (static_cast<size_t>(q) * 4 + i) * kVertexBytes;
            float vx, vy;
            std::memcpy(&vx, vp + 0, sizeof(float));
            std::memcpy(&vy, vp + 4, sizeof(float));
            verts.push_back(x + vx * scale);
            verts.push_back(y + vy * scale);
        }
        const uint32_t base = static_cast<uint32_t>(q) * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean blendWas = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    shader_.setVec3("uResolution",
                    glm::vec3(static_cast<float>(targetW),
                              static_cast<float>(targetH), 0.0f));
    shader_.setVec4("uColor", color);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_DYNAMIC_DRAW);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    if (!blendWas) glDisable(GL_BLEND);
    if (depthWas)  glEnable(GL_DEPTH_TEST);
    if (cullWas)   glEnable(GL_CULL_FACE);
}

}  // namespace render

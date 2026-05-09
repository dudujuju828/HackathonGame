#include "Particles.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>

namespace render {

Particles::~Particles() { release(); }

void Particles::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = 0;
}

bool Particles::init(const std::string& vertPath, const std::string& fragPath) {
    if (!shader_.loadFromFiles(vertPath, fragPath)) return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // Layout matches the GPU-uploaded packed struct: vec3 pos, vec4 col, float size.
    constexpr GLsizei stride = sizeof(float) * (3 + 4 + 1);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(sizeof(float) * 7));
    glBindVertexArray(0);
    return true;
}

void Particles::emit(const Particle& p) {
    particles_.push_back(p);
}

void Particles::update(float dt) {
    for (auto& p : particles_) {
        p.age += dt;
        p.position += p.velocity * dt;
    }
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
            [](const Particle& p){ return p.age >= p.life; }),
        particles_.end());
}

void Particles::draw(const glm::mat4& viewProj, int viewportH) {
    if (particles_.empty()) return;

    // Pack into a flat float buffer matching the vertex layout.
    std::vector<float> buf;
    buf.reserve(particles_.size() * 8);
    for (const auto& p : particles_) {
        const float ageFrac = (p.life > 0.0f) ? (p.age / p.life) : 1.0f;
        const float alpha   = p.color.a * std::max(0.0f, 1.0f - ageFrac);
        buf.push_back(p.position.x);
        buf.push_back(p.position.y);
        buf.push_back(p.position.z);
        buf.push_back(p.color.r);
        buf.push_back(p.color.g);
        buf.push_back(p.color.b);
        buf.push_back(alpha);
        buf.push_back(p.size);
    }

    GLboolean blendWas = glIsEnabled(GL_BLEND);
    GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
    GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    shader_.use();
    shader_.setMat4 ("uViewProj",  viewProj);
    shader_.setFloat("uViewportH", static_cast<float>(viewportH));

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(buf.size() * sizeof(float)),
                 buf.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particles_.size()));
    glBindVertexArray(0);

    glDisable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(GL_TRUE);
    if (!depthWas) glDisable(GL_DEPTH_TEST);
    if (cullWas)   glEnable(GL_CULL_FACE);
    if (!blendWas) glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

}  // namespace render

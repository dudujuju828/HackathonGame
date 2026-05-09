#pragma once

#include "Shader.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace render {

struct Particle {
    glm::vec3 position { 0.0f };
    glm::vec3 velocity { 0.0f };
    glm::vec4 color    { 1.0f, 1.0f, 1.0f, 1.0f };
    float age   = 0.0f;
    float life  = 1.0f;
    float size  = 16.0f;  // base pixels at unit clip-w (perspective-attenuated)
};

// CPU-driven point-sprite particle system. Repacks every frame; fine for
// hundreds of particles, which is well above what the chest effect needs.
class Particles {
public:
    Particles() = default;
    ~Particles();
    Particles(const Particles&) = delete;
    Particles& operator=(const Particles&) = delete;

    bool init(const std::string& vertPath, const std::string& fragPath);

    void emit(const Particle& p);
    void update(float dt);
    // Caller binds the target FBO + viewport.
    void draw(const glm::mat4& viewProj, int viewportH);

    size_t count() const { return particles_.size(); }

private:
    void release();
    Shader shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    std::vector<Particle> particles_;
};

}  // namespace render

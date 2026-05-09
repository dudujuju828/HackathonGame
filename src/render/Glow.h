#pragma once

#include "Shader.h"

#include <glm/glm.hpp>

#include <string>

namespace render {

// Additive ground-disc glow — used for the chest aura. Owns its quad VBO
// and shader; one draw call per disc (cheap at hackathon scale).
class Glow {
public:
    Glow() = default;
    ~Glow();
    Glow(const Glow&) = delete;
    Glow& operator=(const Glow&) = delete;

    bool init(const std::string& vertPath, const std::string& fragPath);

    // Draws an additive disc on the XZ plane centred at `center`.
    // Caller binds the target FBO + viewport. Depth-tests but does not write.
    void draw(const glm::mat4& viewProj,
              const glm::vec3& center,
              float radius,
              const glm::vec3& color,
              float intensity);

private:
    void release();
    Shader shader_;
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
};

}  // namespace render

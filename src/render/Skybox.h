#pragma once

#include "Shader.h"

#include <glm/glm.hpp>

#include <string>

namespace render {

// Equirectangular HDR skybox. Loads an .hdr file into a GL_RGB16F 2D
// texture and samples it via spherical projection in a fullscreen pass.
// Drawn first in the world-render pass with depth-test disabled, so
// subsequent geometry naturally overdraws it.
class Skybox {
public:
    Skybox() = default;
    ~Skybox();
    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    bool init(const std::string& vertPath,
              const std::string& fragPath,
              const std::string& hdrPath);

    // Caller binds the target FBO + viewport. invViewProj is the inverse of
    // (proj * view) for the camera; the shader uses it + uCameraPos to
    // reconstruct world ray directions per pixel.
    void draw(const glm::mat4& invViewProj,
              const glm::vec3& cameraPos,
              float exposure = 1.0f);

private:
    void release();
    Shader        shader_;
    unsigned int  vao_      = 0;
    unsigned int  vbo_      = 0;
    unsigned int  hdrTex_   = 0;
};

}  // namespace render

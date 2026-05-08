#pragma once

#include "Shader.h"

#include <glm/glm.hpp>
#include <string>

namespace render {

// Tunable parameters for the CRT post effect. Defaults aim at
// "moderate horror VHS" — visible but not nauseating.
struct CrtParams {
    float curvature        = 0.12f;   // 0 = flat, 0.15+ = bulgy
    float scanlineStrength = 0.50f;   // 0..1
    float scanlineCount    = 280.0f;  // virtual scanlines (resolution-independent)
    float aberrationPx     = 2.5f;    // RGB shift in pixels at screen edge
    float vignette         = 0.85f;   // 0..1
    float noise            = 0.10f;   // 0..1, animated grain
    float flicker          = 1.2f;    // 0..2, brightness wobble + dropouts
    float maskStrength     = 0.18f;   // 0..1, RGB phosphor sub-pixel tint
};

class PostFx {
public:
    PostFx() = default;
    ~PostFx();

    PostFx(const PostFx&) = delete;
    PostFx& operator=(const PostFx&) = delete;

    bool init(const std::string& vertPath, const std::string& fragPath);

    // Draws a full-screen quad sampling sceneTexture, applying the CRT effect.
    // Caller is responsible for binding the target framebuffer + viewport first.
    void apply(unsigned int sceneTexture,
               int   targetW,
               int   targetH,
               float timeSeconds,
               const CrtParams& p);

    CrtParams& params() { return params_; }
    const CrtParams& params() const { return params_; }

private:
    void release();

    Shader        shader_;
    unsigned int  vao_    = 0;
    unsigned int  vbo_    = 0;
    CrtParams     params_ {};
};

}  // namespace render

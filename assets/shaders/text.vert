#version 330 core
layout(location = 0) in vec2 aPos;

uniform vec3 uResolution;

void main() {
    // aPos is in pixels with top-left origin; map to NDC.
    vec2 ndc = vec2(aPos.x / uResolution.x * 2.0 - 1.0,
                    1.0 - aPos.y / uResolution.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}

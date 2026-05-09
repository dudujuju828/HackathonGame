#version 330 core
layout(location = 0) in vec2 aPos;  // unit-quad corners in [-1, 1]

uniform vec3  uResolution;  // (W, H, _) in pixels
uniform vec3  uCenter;      // pixel-space centre, top-left origin (z unused)
uniform float uRadius;      // pixel radius

out vec2 vLocal;

void main() {
    vLocal = aPos;
    vec2 p = uCenter.xy + aPos * uRadius;
    vec2 ndc = vec2(p.x / uResolution.x * 2.0 - 1.0,
                    1.0 - p.y / uResolution.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}

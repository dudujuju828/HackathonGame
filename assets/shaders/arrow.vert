#version 330 core
layout(location = 0) in vec2 aPos;  // local triangle vertex (tip points +X)

uniform vec3  uResolution;  // (W, H, _)
uniform vec3  uCenter;      // pixel coords, top-left origin (z unused)
uniform float uAngle;       // radians; rotates around uCenter
uniform float uSize;        // pixel scale

void main() {
    float c = cos(uAngle);
    float s = sin(uAngle);
    vec2 rotated = vec2(aPos.x * c - aPos.y * s,
                        aPos.x * s + aPos.y * c);
    vec2 p = uCenter.xy + rotated * uSize;
    vec2 ndc = vec2(p.x / uResolution.x * 2.0 - 1.0,
                    1.0 - p.y / uResolution.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}

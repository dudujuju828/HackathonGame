#version 330 core
layout(location = 0) in vec2 aPos;  // fullscreen quad in NDC

out vec2 vScreenPos;

void main() {
    vScreenPos = aPos;
    // z=1 puts us at the far plane so the inverse view-proj reconstructs
    // a world position out at infinity, which is just what we need for a sky.
    gl_Position = vec4(aPos, 1.0, 1.0);
}

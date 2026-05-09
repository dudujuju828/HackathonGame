#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in float aSize;  // base pixel size at unit clip-w

uniform mat4  uViewProj;
uniform float uViewportH;  // pixels — drives the perspective size attenuation

out vec4 vColor;

void main() {
    vec4 clip = uViewProj * vec4(aPos, 1.0);
    gl_Position = clip;
    gl_PointSize = aSize * uViewportH / max(clip.w, 0.001);
    vColor = aColor;
}

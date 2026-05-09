#version 330 core
layout(location = 0) in vec2 aQuad;  // quad corners in [-1, 1]

uniform mat4  uViewProj;
uniform vec3  uCenter;   // world-space chest position (Y is ground height)
uniform float uRadius;   // metres

out vec2 vUV;

void main() {
    vUV = aQuad;
    vec3 world = vec3(uCenter.x + aQuad.x * uRadius,
                      uCenter.y,
                      uCenter.z + aQuad.y * uRadius);
    gl_Position = uViewProj * vec4(world, 1.0);
}

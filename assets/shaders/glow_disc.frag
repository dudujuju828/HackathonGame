#version 330 core
in vec2 vUV;

uniform vec3  uColor;
uniform float uIntensity;

out vec4 FragColor;

void main() {
    float d = length(vUV);
    if (d > 1.0) discard;
    // Sharp at the centre, soft tail toward the edge.
    float a = pow(1.0 - d, 2.5);
    FragColor = vec4(uColor * uIntensity, a);
}

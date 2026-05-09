#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main() {
    vec2 c = gl_PointCoord - 0.5;
    float d = length(c);
    if (d > 0.5) discard;
    float a = smoothstep(0.5, 0.0, d);
    FragColor = vec4(vColor.rgb, vColor.a * a);
}

#version 330 core
in vec2 vLocal;            // -1..1 within the disc

uniform vec4  uFillColor;
uniform vec4  uBorderColor;
uniform float uBorderFrac; // 0..1 fraction of radius reserved for the border ring

out vec4 FragColor;

void main() {
    float d = length(vLocal);
    if (d > 1.0) discard;
    float aa = fwidth(d);
    if (d > 1.0 - uBorderFrac) {
        // soft outer edge
        float fade = smoothstep(1.0, 1.0 - aa, d);
        FragColor = vec4(uBorderColor.rgb, uBorderColor.a * fade);
    } else {
        FragColor = uFillColor;
    }
}

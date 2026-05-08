#version 330 core

uniform vec3  uResolution;
uniform vec3  uColor;
uniform float uSize;       // arm length from center, px
uniform float uThickness;  // arm full width, px
uniform float uGap;        // center gap radius, px
uniform float uOutline;    // outline width, px
uniform float uOpacity;

out vec4 FragColor;

float sdRect(vec2 p, vec2 halfSize) {
    vec2 d = abs(p) - halfSize;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

void main() {
    vec2 p = gl_FragCoord.xy - uResolution.xy * 0.5;

    // Plus-sign SDF: union of horizontal and vertical bars.
    float dH = sdRect(p, vec2(uSize, uThickness * 0.5));
    float dV = sdRect(p, vec2(uThickness * 0.5, uSize));
    float dCross = min(dH, dV);

    // Subtract central gap (signed circle inverted).
    float dGap = uGap - length(p);          // > 0 inside gap
    float d    = max(dCross, dGap);         // inside cross AND outside gap

    // Two AA bands: the colored core and a darker outline ring.
    float aa = 1.0;  // ~1 px AA
    float core    = 1.0 - smoothstep(-aa,            aa,            d);
    float outline = 1.0 - smoothstep(uOutline - aa,  uOutline + aa, d);

    if (outline < 0.001) discard;

    vec3 col = mix(vec3(0.0), uColor, core);
    FragColor = vec4(col, outline * uOpacity);
}

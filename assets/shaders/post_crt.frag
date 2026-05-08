#version 330 core

in vec2 vUV;

uniform sampler2D uScene;
uniform vec3  uResolution;       // (width, height, _) in pixels
uniform float uTime;
uniform float uCurvature;        // 0 = flat, 0.15 = strong barrel
uniform float uScanlineStrength; // 0..1
uniform float uScanlineCount;    // virtual scanline rows (e.g. 280)
uniform float uAberrationPx;     // pixel offset for chroma split at edge
uniform float uVignette;         // 0..1
uniform float uNoise;            // 0..1
uniform float uFlicker;          // 0..1
uniform float uMaskStrength;     // 0..1

out vec4 FragColor;

vec2 barrel(vec2 uv, float k) {
    uv = uv * 2.0 - 1.0;
    vec2 off = abs(uv.yx) / vec2(6.0, 5.0);
    uv = uv + uv * off * off * k;
    return uv * 0.5 + 0.5;
}

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec2 uv = barrel(vUV, uCurvature);

    // Outside the warped screen = bezel black.
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Chromatic aberration: shift R/B radially from center.
    vec2 dir = uv - 0.5;
    float ab = uAberrationPx / max(uResolution.x, 1.0);
    float r = texture(uScene, uv + dir * ab * 2.0).r;
    float g = texture(uScene, uv).g;
    float b = texture(uScene, uv - dir * ab * 2.0).b;
    vec3 col = vec3(r, g, b);

    // Scanlines — virtual line count keeps density consistent across resolutions.
    float scan = sin(uv.y * uScanlineCount * 3.14159265) * 0.5 + 0.5;
    col *= mix(1.0, scan, uScanlineStrength);

    // Subtle RGB phosphor mask in screen-space columns.
    float mx = mod(gl_FragCoord.x, 3.0);
    vec3 mask = vec3(1.0);
    if      (mx < 1.0) mask = vec3(1.0 + uMaskStrength, 1.0 - uMaskStrength * 0.5, 1.0 - uMaskStrength * 0.5);
    else if (mx < 2.0) mask = vec3(1.0 - uMaskStrength * 0.5, 1.0 + uMaskStrength, 1.0 - uMaskStrength * 0.5);
    else                mask = vec3(1.0 - uMaskStrength * 0.5, 1.0 - uMaskStrength * 0.5, 1.0 + uMaskStrength);
    col *= mask;

    // Vignette: darken radially.
    float vig = 1.0 - dot(dir, dir) * uVignette * 1.6;
    col *= clamp(vig, 0.0, 1.0);

    // Animated grain.
    float n = hash21(gl_FragCoord.xy + vec2(uTime * 73.0, uTime * 37.0));
    col += (n - 0.5) * uNoise;

    // Active flicker: constant wobble + occasional sharp dropouts.
    // Wobble is a beat between two low-freq sines so the breathing isn't periodic.
    float wobble = (sin(uTime * 13.7) + sin(uTime * 7.3)) * 0.04;
    // Dropouts: per-time-bucket hash; only the brightest hashes trigger a dim.
    float bucket = floor(uTime * 24.0);
    float h      = hash21(vec2(bucket, 7.0));
    float drop   = smoothstep(0.86, 0.96, h) * 0.35;
    // Brief horizontal "tear" band when a dropout triggers.
    float tearBand = smoothstep(0.02, 0.0, abs(uv.y - hash21(vec2(bucket, 13.0))));
    float tear     = drop * tearBand * 0.6;
    float fk = 1.0 + (wobble - drop - tear) * uFlicker;
    col *= max(fk, 0.0);

    FragColor = vec4(col, 1.0);
}

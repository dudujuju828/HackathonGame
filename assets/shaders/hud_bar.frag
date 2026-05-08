#version 330 core

uniform vec3  uResolution;
uniform vec3  uOrigin;       // (x, y, _) bottom-left of inner bar in px
uniform vec3  uSize;         // (w, h, _) inner bar size in px
uniform float uFill;         // 0..1
uniform int   uSegments;     // 0 = continuous, else number of cells
uniform vec3  uFillColor;
uniform vec3  uEmptyColor;
uniform vec3  uBorderColor;
uniform float uBorderPx;
uniform float uPulse;        // 0..1, lightens fill (e.g. low-HP throb)
uniform float uOpacity;

out vec4 FragColor;

void main() {
    // gl_FragCoord is bottom-left origin; we expose a top-left coord
    // system so HUD layout matches GLFW mouse and the text shader.
    vec2 fragTL = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
    vec2 p  = fragTL - uOrigin.xy;
    vec2 sz = uSize.xy;

    // Outside outer (border-inclusive) box -> nothing to draw.
    if (p.x < -uBorderPx || p.y < -uBorderPx ||
        p.x > sz.x + uBorderPx || p.y > sz.y + uBorderPx) {
        discard;
    }

    // Border ring.
    if (p.x < 0.0 || p.y < 0.0 || p.x > sz.x || p.y > sz.y) {
        FragColor = vec4(uBorderColor, uOpacity);
        return;
    }

    bool isGap     = false;
    bool cellLit   = false;

    if (uSegments > 0) {
        float cellW   = sz.x / float(uSegments);
        float fx      = p.x / cellW;
        int   cell    = int(floor(fx));
        float xInCell = (fx - float(cell)) * cellW;

        // 1.5px gap on the trailing edge of every cell except the last.
        if (cell < uSegments - 1 && xInCell > cellW - 1.5) {
            isGap = true;
        }
        int filledCells = int(round(uFill * float(uSegments)));
        cellLit = cell < filledCells;
    } else {
        cellLit = p.x <= sz.x * uFill;
    }

    if (isGap) {
        FragColor = vec4(uEmptyColor * 0.5, uOpacity);
        return;
    }

    vec3 col = cellLit
        ? mix(uFillColor, vec3(1.0), uPulse * 0.6)
        : uEmptyColor;
    FragColor = vec4(col, uOpacity);
}

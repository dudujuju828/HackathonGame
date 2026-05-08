#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uAlbedo;
uniform vec3 uTint;        // multiplied with albedo (vec3(1) for no-op)
uniform vec3 uCamPos;
uniform vec3 uCamDir;
uniform vec3 uAmbient;
uniform float uFlashInner; // cos(inner angle)
uniform float uFlashOuter; // cos(outer angle)
uniform vec3 uFlashColor;

out vec4 FragColor;

void main() {
    vec3 albedo = texture(uAlbedo, vUV).rgb * uTint;
    vec3 N = normalize(vNormal);

    vec3 toFrag = vWorldPos - uCamPos;
    float dist = length(toFrag);
    vec3 dirToFrag = toFrag / max(dist, 0.0001);
    vec3 L = -dirToFrag;

    float theta = dot(dirToFrag, normalize(uCamDir));
    float cone = clamp((theta - uFlashOuter) / max(uFlashInner - uFlashOuter, 0.0001), 0.0, 1.0);
    float atten = 1.0 / (1.0 + 0.05 * dist + 0.01 * dist * dist);
    float ndotl = max(dot(N, L), 0.0);

    vec3 lit = albedo * uAmbient
             + albedo * uFlashColor * cone * atten * ndotl;

    FragColor = vec4(lit, 1.0);
}

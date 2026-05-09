#version 330 core
in vec2 vScreenPos;

uniform mat4      uInvViewProj;
uniform vec3      uCameraPos;
uniform sampler2D uSky;
uniform float     uExposure;  // multiplier on the HDR radiance pre-tonemap

out vec4 FragColor;

const float kPi = 3.14159265358979;

void main() {
    // Reconstruct world position at the far plane through inverse view-proj,
    // then build a normalised view ray from the camera.
    vec4 wp = uInvViewProj * vec4(vScreenPos, 1.0, 1.0);
    wp /= wp.w;
    vec3 dir = normalize(wp.xyz - uCameraPos);

    // Equirectangular spherical UV (Y up, longitude/latitude).
    float u = atan(dir.z, dir.x) / (2.0 * kPi) + 0.5;
    float v = asin(clamp(dir.y, -1.0, 1.0)) / kPi + 0.5;

    vec3 col = texture(uSky, vec2(u, v)).rgb * uExposure;
    // Reinhard tonemap so blown-out HDR areas don't clip to pure white.
    col = col / (col + vec3(1.0));
    FragColor = vec4(col, 1.0);
}

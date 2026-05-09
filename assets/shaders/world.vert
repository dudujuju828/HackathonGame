#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in ivec4 aBoneIds;
layout(location = 4) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;

// Bumped from 100 — Cat.glb has a 175-joint rig; out-of-range vertex
// boneIds against a too-small array read garbage and collapse vertices.
const int MAX_BONES = 200;
uniform mat4 uBones[MAX_BONES];
uniform bool uHasBones;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

void main() {
    vec4 localPos;
    vec3 localNormal;

    if (uHasBones) {
        mat4 boneTransform = uBones[aBoneIds[0]] * aBoneWeights[0];
        boneTransform     += uBones[aBoneIds[1]] * aBoneWeights[1];
        boneTransform     += uBones[aBoneIds[2]] * aBoneWeights[2];
        boneTransform     += uBones[aBoneIds[3]] * aBoneWeights[3];

        localPos = boneTransform * vec4(aPos, 1.0);
        localNormal = mat3(boneTransform) * aNormal;
    } else {
        localPos = vec4(aPos, 1.0);
        localNormal = aNormal;
    }

    vec4 world = uModel * localPos;
    vWorldPos = world.xyz;
    vNormal = mat3(uModel) * localNormal;
    vUV = aUV;
    gl_Position = uViewProj * world;
}

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace render {

// A single bone in the skeletal hierarchy.
struct Joint {
    std::string name;
    int         parentIndex = -1; // -1 if this is a root joint
    glm::mat4   inverseBindMatrix { 1.0f };

    // The rest pose local transform (useful if an animation channel only overrides e.g. rotation)
    glm::vec3   translation { 0.0f };
    glm::quat   rotation    { 1.0f, 0.0f, 0.0f, 0.0f }; // w, x, y, z
    glm::vec3   scale       { 1.0f };
};

// Represents the full hierarchy of bones for a model.
struct Skeleton {
    std::vector<Joint> joints;
};

template<typename T>
struct Keyframe {
    float time;
    T     value;
};

// All the translation, rotation, and scale keyframes for a single joint.
struct AnimationChannel {
    int targetJoint = -1; // Index into Skeleton::joints

    std::vector<Keyframe<glm::vec3>> translations;
    std::vector<Keyframe<glm::quat>> rotations;
    std::vector<Keyframe<glm::vec3>> scales;
};

// A full animation (e.g. "Walk", "Run") containing channels for various joints.
struct AnimationClip {
    std::string name;
    float       duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

}  // namespace render
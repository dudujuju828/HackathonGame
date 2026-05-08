#pragma once

#include "Animation.h"
#include <glm/glm.hpp>
#include <vector>

namespace render {

class Animator {
public:
    void setAnimation(const AnimationClip* clip);
    void update(float dt);

    void calculateBoneTransforms(const Skeleton* skeleton);

    const std::vector<glm::mat4>& finalBoneMatrices() const { return finalBoneMatrices_; }

private:
    const AnimationClip* currentClip_ = nullptr;
    float currentTime_ = 0.0f;
    std::vector<glm::mat4> finalBoneMatrices_;
    std::vector<glm::mat4> globalTransforms_;

    glm::vec3 interpolateTranslation(float time, const AnimationChannel& channel);
    glm::quat interpolateRotation(float time, const AnimationChannel& channel);
    glm::vec3 interpolateScale(float time, const AnimationChannel& channel);
};

}  // namespace render
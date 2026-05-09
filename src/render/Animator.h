#pragma once

#include "Animation.h"
#include <glm/glm.hpp>
#include <vector>

namespace render {

class Animator {
public:
    // Loop=true (default) wraps with fmod; loop=false clamps at duration so
    // the final pose holds — useful for one-shot opens, deaths, etc.
    void setAnimation(const AnimationClip* clip, bool loop = true);
    void update(float dt);

    void calculateBoneTransforms(const Skeleton* skeleton);

    const std::vector<glm::mat4>& finalBoneMatrices() const { return finalBoneMatrices_; }

    float currentTime() const { return currentTime_; }
    bool  finished() const {
        return currentClip_ && !loop_ && currentTime_ >= currentClip_->duration;
    }

private:
    const AnimationClip* currentClip_ = nullptr;
    float currentTime_ = 0.0f;
    bool  loop_        = true;
    std::vector<glm::mat4> finalBoneMatrices_;
    std::vector<glm::mat4> globalTransforms_;

    glm::vec3 interpolateTranslation(float time, const AnimationChannel& channel);
    glm::quat interpolateRotation(float time, const AnimationChannel& channel);
    glm::vec3 interpolateScale(float time, const AnimationChannel& channel);
};

}  // namespace render
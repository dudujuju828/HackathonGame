#include "Animator.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace render {

void Animator::setAnimation(const AnimationClip* clip, bool loop) {
    currentClip_ = clip;
    currentTime_ = 0.0f;
    loop_        = loop;
}

void Animator::update(float dt) {
    if (!currentClip_ || currentClip_->duration <= 0.0f) return;
    currentTime_ += dt;
    if (loop_) {
        currentTime_ = std::fmod(currentTime_, currentClip_->duration);
    } else if (currentTime_ > currentClip_->duration) {
        currentTime_ = currentClip_->duration;  // clamp; pose holds at end
    }
}

// All three interpolators share the same shape: handle end/empty/single-key
// edge cases up front, then walk the keyframes to bracket `time`. The end-hold
// guard is critical — without it, time>=last keyframe falls through the
// search loop and we snap back to interpolating between keyframe 0 and 1.

glm::vec3 Animator::interpolateTranslation(float time, const AnimationChannel& channel) {
    if (channel.translations.empty()) return glm::vec3(0.0f);
    if (channel.translations.size() == 1) return channel.translations[0].value;
    if (time >= channel.translations.back().time) return channel.translations.back().value;

    int p0 = 0;
    for (int i = 0; i < static_cast<int>(channel.translations.size()) - 1; ++i) {
        if (time < channel.translations[i + 1].time) {
            p0 = i;
            break;
        }
    }
    int p1 = p0 + 1;

    float t0 = channel.translations[p0].time;
    float t1 = channel.translations[p1].time;
    float factor = std::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f);

    return glm::mix(channel.translations[p0].value, channel.translations[p1].value, factor);
}

glm::quat Animator::interpolateRotation(float time, const AnimationChannel& channel) {
    if (channel.rotations.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (channel.rotations.size() == 1) return channel.rotations[0].value;
    if (time >= channel.rotations.back().time) return channel.rotations.back().value;

    int p0 = 0;
    for (int i = 0; i < static_cast<int>(channel.rotations.size()) - 1; ++i) {
        if (time < channel.rotations[i + 1].time) {
            p0 = i;
            break;
        }
    }
    int p1 = p0 + 1;

    float t0 = channel.rotations[p0].time;
    float t1 = channel.rotations[p1].time;
    float factor = std::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f);

    return glm::slerp(channel.rotations[p0].value, channel.rotations[p1].value, factor);
}

glm::vec3 Animator::interpolateScale(float time, const AnimationChannel& channel) {
    if (channel.scales.empty()) return glm::vec3(1.0f);
    if (channel.scales.size() == 1) return channel.scales[0].value;
    if (time >= channel.scales.back().time) return channel.scales.back().value;

    int p0 = 0;
    for (int i = 0; i < static_cast<int>(channel.scales.size()) - 1; ++i) {
        if (time < channel.scales[i + 1].time) {
            p0 = i;
            break;
        }
    }
    int p1 = p0 + 1;

    float t0 = channel.scales[p0].time;
    float t1 = channel.scales[p1].time;
    float factor = std::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f);

    return glm::mix(channel.scales[p0].value, channel.scales[p1].value, factor);
}

void Animator::calculateBoneTransforms(const Skeleton* skeleton) {
    if (!skeleton || skeleton->joints.empty()) {
        finalBoneMatrices_.clear();
        return;
    }

    if (globalTransforms_.size() < skeleton->joints.size()) {
        globalTransforms_.resize(skeleton->joints.size(), glm::mat4(1.0f));
    }
    if (finalBoneMatrices_.size() < skeleton->joints.size()) {
        finalBoneMatrices_.resize(skeleton->joints.size(), glm::mat4(1.0f));
    }

    // Traverse the flat hierarchy. cgltf guarantees parents appear before children,
    // so a flat loop is sufficient.
    for (int i = 0; i < static_cast<int>(skeleton->joints.size()); ++i) {
        const Joint& joint = skeleton->joints[i];

        glm::vec3 t = joint.translation;
        glm::quat r = joint.rotation;
        glm::vec3 s = joint.scale;

        if (currentClip_) {
            for (const auto& chan : currentClip_->channels) {
                if (chan.targetJoint == i) {
                    if (!chan.translations.empty()) t = interpolateTranslation(currentTime_, chan);
                    if (!chan.rotations.empty())    r = interpolateRotation(currentTime_, chan);
                    if (!chan.scales.empty())       s = interpolateScale(currentTime_, chan);
                    break;
                }
            }
        }

        glm::mat4 local = glm::translate(glm::mat4(1.0f), t) *
                          glm::mat4_cast(r) *
                          glm::scale(glm::mat4(1.0f), s);

        if (joint.parentIndex >= 0) {
            globalTransforms_[i] = globalTransforms_[joint.parentIndex] * local;
        } else {
            globalTransforms_[i] = local;
        }

        finalBoneMatrices_[i] = globalTransforms_[i] * joint.inverseBindMatrix;
    }
}

}  // namespace render
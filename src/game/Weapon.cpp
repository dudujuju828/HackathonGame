#include "Weapon.h"

#include "Player.h"
#include "core/Input.h"
#include "render/Camera.h"
#include "render/Model.h"
#include "render/Shader.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace game {

bool Weapon::update(float dt, const core::Input& input, Player& player) {
    firedThisFrame_ = false;

    cooldown_ = std::max(0.0f, cooldown_ - dt);
    kickback_ = std::max(0.0f, kickback_ - kickDecay_ * dt);

    // Walking bob — phase advances with horizontal speed (steps per metre),
    // intensity smooths in/out so transitions don't pop.
    const glm::vec3 v = player.velocity();
    const float speedXZ = std::sqrt(v.x * v.x + v.z * v.z);
    bobPhase_ += speedXZ * 0.35f * dt;       // matches Player's bobStepRate
    if (bobPhase_ > 1e6f) bobPhase_ = std::fmod(bobPhase_, 1.0f);
    const float target = (speedXZ > 0.05f) ? 1.0f : 0.0f;
    bobIntensity_ = glm::mix(bobIntensity_, target, std::min(8.0f * dt, 1.0f));

    // Handle ongoing burst.
    if (burstRemaining_ > 0) {
        burstTimer_ -= dt;
        if (burstTimer_ <= 0.0f) {
            burstTimer_ += burstInterval;
            burstRemaining_--;
            firedThisFrame_ = true;
            kickback_ = 1.0f;
            player.addTrauma(traumaPerShot * 0.5f); // smaller shake for follow-up shots
        }
    }

    const bool click       = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
    const bool justClicked = click && !prevClick_;
    prevClick_ = click;

    const bool fireTrigger = autoFire ? click : justClicked;

    // Trigger new shot/burst if not already bursting and cooldown is ready.
    if (fireTrigger && cooldown_ <= 0.0f && burstRemaining_ <= 0 && (unlimited || ammo > 0)) {
        if (!unlimited) ammo -= 1;
        cooldown_  = fireRate;
        kickback_  = 1.0f;
        player.addTrauma(traumaPerShot);
        firedThisFrame_ = true;

        if (projectileCount > 1) {
            burstRemaining_ = projectileCount - 1;
            burstTimer_ = burstInterval;
        }
    }
    return firedThisFrame_;
}

glm::mat4 Weapon::worldMatrix_(const render::Camera& cam) const {
    const glm::vec3 fwd   = cam.forward();
    const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
    const glm::vec3 up    = glm::cross(right, fwd);

    glm::mat4 viewToWorld(1.0f);
    viewToWorld[0] = glm::vec4(right,         0.0f);
    viewToWorld[1] = glm::vec4(up,            0.0f);
    viewToWorld[2] = glm::vec4(-fwd,          0.0f);
    viewToWorld[3] = glm::vec4(cam.position,  1.0f);

    const float k = kickback_;

    // Walk bob: vertical sin at step rate, in/out cosine at half rate so
    // the syringe drifts forward/back over the step cadence.
    constexpr float kTwoPi   = 6.28318530718f;
    constexpr float kBobAmpY = 0.022f;  // metres
    constexpr float kBobAmpZ = 0.030f;  // metres
    const float bobY =  std::sin(bobPhase_ * kTwoPi)        * kBobAmpY * bobIntensity_;
    const float bobZ =  std::sin(bobPhase_ * kTwoPi * 0.5f) * kBobAmpZ * bobIntensity_;

    // View-space: +X right, +Y up, -Z forward (into scene). The user-facing
    // offset.z is "distance in front", flipped to view-space here.
    const glm::vec3 viewOffset(
        viewmodel.offset.x,
        viewmodel.offset.y + k * 0.025f + bobY,             // small upward thrust + bob
        -(viewmodel.offset.z + k * 0.10f + bobZ)            // stab forward + in/out drift
    );

    glm::mat4 local(1.0f);
    local = glm::translate(local, viewOffset);
    local = glm::rotate(local, glm::radians(viewmodel.rotation.y),               glm::vec3(0, 1, 0));
    local = glm::rotate(local, glm::radians(viewmodel.rotation.x - k * 8.0f),    glm::vec3(1, 0, 0));
    local = glm::rotate(local, glm::radians(viewmodel.rotation.z + k * 6.0f),    glm::vec3(0, 0, 1));
    local = glm::scale(local, glm::vec3(viewmodel.scale));

    return viewToWorld * local;
}

void Weapon::draw(render::Shader& worldShader, const render::Camera& cam) const {
    if (!model_) return;

    // Many imported meshes have CW winding; disable culling so we don't
    // see-through the syringe.
    GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);

    const glm::mat4 M = worldMatrix_(cam);
    worldShader.setMat4("uModel", M);
    for (const auto& sub : model_->meshes()) {
        if (sub.diffuse) sub.diffuse->bind(0);
        sub.mesh.draw();
    }

    if (cullWas) glEnable(GL_CULL_FACE);
}

}  // namespace game

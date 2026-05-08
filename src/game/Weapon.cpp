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

    const bool click       = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
    const bool justClicked = click && !prevClick_;
    prevClick_ = click;

    if (justClicked && cooldown_ <= 0.0f && ammo > 0) {
        ammo      -= 1;
        cooldown_  = fireRate;
        kickback_  = 1.0f;
        player.addTrauma(traumaPerShot);
        firedThisFrame_ = true;
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

    // View-space: +X right, +Y up, -Z forward (into scene). The user-facing
    // offset.z is "distance in front", flipped to view-space here.
    const glm::vec3 viewOffset(
        viewmodel.offset.x,
        viewmodel.offset.y + k * 0.025f,           // small upward thrust
        -(viewmodel.offset.z + k * 0.10f)          // syringe stabs forward
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

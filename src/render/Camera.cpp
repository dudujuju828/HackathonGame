#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace render {

void Camera::addLook(float dxPixels, float dyPixels, float sensitivity) {
    yaw   += dxPixels * sensitivity;
    pitch -= dyPixels * sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}

glm::vec3 Camera::forward() const {
    float cy = std::cos(glm::radians(yaw));
    float sy = std::sin(glm::radians(yaw));
    float cp = std::cos(glm::radians(pitch));
    float sp = std::sin(glm::radians(pitch));
    return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::proj(float aspect) const {
    return glm::perspective(glm::radians(fovDeg), aspect, znear, zfar);
}

}  // namespace render

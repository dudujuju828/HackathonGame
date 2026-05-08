#pragma once

#include <glm/glm.hpp>

namespace render {

class Camera {
public:
    glm::vec3 position { 0.0f, 1.6f, 3.0f };
    float yaw   = -90.0f;  // degrees, -90 looks toward -Z
    float pitch = 0.0f;

    // View-only offsets in degrees (camera shake). Added to yaw/pitch
    // when forming the view direction; do NOT affect movement basis.
    float viewShakeYaw   = 0.0f;
    float viewShakePitch = 0.0f;

    float fovDeg = 70.0f;
    float znear  = 0.05f;
    float zfar   = 200.0f;

    void addLook(float dxPixels, float dyPixels, float sensitivity = 0.1f);

    glm::vec3 forward() const;
    glm::vec3 right()   const;
    glm::vec3 up()      const;

    glm::mat4 view() const;
    glm::mat4 proj(float aspect) const;
};

}  // namespace render

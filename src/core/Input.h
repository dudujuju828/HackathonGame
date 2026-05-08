#pragma once

struct GLFWwindow;

namespace core {

class Input {
public:
    void attach(GLFWwindow* window);
    void update();

    bool key(int glfwKey) const;
    bool mouseButton(int glfwButton) const;

    float mouseDX() const { return dx_; }
    float mouseDY() const { return dy_; }

    double mouseX() const { return lastX_; }
    double mouseY() const { return lastY_; }

    // Discards the next mouse delta. Call this when toggling cursor lock so
    // the Player doesn't get yanked by a stale absolute jump.
    void resetMouseDelta() { firstSample_ = true; dx_ = dy_ = 0.0f; }

private:
    GLFWwindow* window_ = nullptr;
    double lastX_ = 0.0;
    double lastY_ = 0.0;
    float  dx_ = 0.0f;
    float  dy_ = 0.0f;
    bool   firstSample_ = true;
};

}  // namespace core

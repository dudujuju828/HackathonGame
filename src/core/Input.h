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

private:
    GLFWwindow* window_ = nullptr;
    double lastX_ = 0.0;
    double lastY_ = 0.0;
    float  dx_ = 0.0f;
    float  dy_ = 0.0f;
    bool   firstSample_ = true;
};

}  // namespace core

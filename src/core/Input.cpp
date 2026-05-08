#include "Input.h"

#include <GLFW/glfw3.h>

namespace core {

void Input::attach(GLFWwindow* window) {
    window_ = window;
    firstSample_ = true;
    dx_ = dy_ = 0.0f;
}

void Input::update() {
    if (!window_) return;

    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    if (firstSample_) {
        lastX_ = x;
        lastY_ = y;
        firstSample_ = false;
    }
    dx_ = static_cast<float>(x - lastX_);
    dy_ = static_cast<float>(y - lastY_);
    lastX_ = x;
    lastY_ = y;
}

bool Input::key(int glfwKey) const {
    return window_ && glfwGetKey(window_, glfwKey) == GLFW_PRESS;
}

bool Input::mouseButton(int glfwButton) const {
    return window_ && glfwGetMouseButton(window_, glfwButton) == GLFW_PRESS;
}

}  // namespace core

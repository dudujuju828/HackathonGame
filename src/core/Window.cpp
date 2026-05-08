#include "Window.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <stdexcept>

namespace core {

void Window::framebufferResized(GLFWwindow* w, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
    if (self) {
        self->width_  = width;
        self->height_ = height;
    }
    glViewport(0, 0, width, height);
}

Window::Window(int width, int height, const char* title)
    : width_(width), height_(height) {
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, &Window::framebufferResized);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("gladLoadGL failed");
    }

    std::printf("[gl] %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glViewport(0, 0, width, height);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void Window::swap() {
    glfwSwapBuffers(window_);
}

void Window::pollEvents() {
    glfwPollEvents();
}

}  // namespace core

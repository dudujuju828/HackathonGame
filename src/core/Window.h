#pragma once

struct GLFWwindow;

namespace core {

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void swap();
    void pollEvents();

    GLFWwindow* handle() const { return window_; }
    int width()  const { return width_; }
    int height() const { return height_; }
    float aspect() const { return height_ > 0 ? float(width_) / float(height_) : 1.0f; }

private:
    static void framebufferResized(GLFWwindow* w, int width, int height);

    GLFWwindow* window_ = nullptr;
    int width_  = 0;
    int height_ = 0;
};

}  // namespace core

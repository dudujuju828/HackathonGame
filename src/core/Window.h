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

    // Toggle between windowed and primary-monitor fullscreen. Cheap to call
    // every frame — short-circuits if already in the requested mode.
    void setFullscreen(bool fs);
    bool isFullscreen() const { return fullscreen_; }

    GLFWwindow* handle() const { return window_; }
    int width()  const { return width_; }
    int height() const { return height_; }
    float aspect() const { return height_ > 0 ? float(width_) / float(height_) : 1.0f; }

private:
    static void framebufferResized(GLFWwindow* w, int width, int height);

    GLFWwindow* window_ = nullptr;
    int width_  = 0;
    int height_ = 0;

    bool fullscreen_ = false;
    int  windowedX_  = 100;
    int  windowedY_  = 100;
    int  windowedW_  = 1280;
    int  windowedH_  = 720;
};

}  // namespace core

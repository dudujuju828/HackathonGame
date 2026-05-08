#include "Time.h"

#include <GLFW/glfw3.h>

namespace core {

void Time::tick() {
    float now = static_cast<float>(glfwGetTime());
    if (!started_) {
        last_ = now;
        started_ = true;
    }
    dt_    = now - last_;
    last_  = now;
    total_ = now;
    if (dt_ > 0.1f) dt_ = 0.1f;  // clamp on hiccups
}

}  // namespace core

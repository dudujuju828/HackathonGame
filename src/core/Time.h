#pragma once

namespace core {

class Time {
public:
    void tick();
    float dt()    const { return dt_; }
    float total() const { return total_; }

private:
    float last_  = 0.0f;
    float dt_    = 0.0f;
    float total_ = 0.0f;
    bool  started_ = false;
};

}  // namespace core

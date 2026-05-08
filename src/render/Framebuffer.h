#pragma once

namespace render {

// Color (RGBA8) + depth/stencil renderbuffer FBO. Resizes on demand.
class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void resize(int width, int height);
    void bind() const;
    static void bindDefault(int viewportW, int viewportH);

    unsigned int colorTexture() const { return color_; }
    int width()  const { return width_; }
    int height() const { return height_; }

private:
    void release();

    unsigned int fbo_     = 0;
    unsigned int color_   = 0;
    unsigned int depth_   = 0;
    int          width_   = 0;
    int          height_  = 0;
};

}  // namespace render

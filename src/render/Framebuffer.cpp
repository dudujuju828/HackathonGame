#include "Framebuffer.h"

#include <glad/gl.h>
#include <cstdio>

namespace render {

Framebuffer::~Framebuffer() { release(); }

void Framebuffer::release() {
    if (fbo_)   glDeleteFramebuffers(1, &fbo_);
    if (color_) glDeleteTextures(1, &color_);
    if (depth_) glDeleteRenderbuffers(1, &depth_);
    fbo_ = color_ = depth_ = 0;
    width_ = height_ = 0;
}

void Framebuffer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == width_ && height == height_ && fbo_ != 0) return;

    release();

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &color_);
    glBindTexture(GL_TEXTURE_2D, color_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, color_, 0);

    glGenRenderbuffers(1, &depth_);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, depth_);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[fbo] incomplete: 0x%X\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    width_  = width;
    height_ = height;
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
}

void Framebuffer::bindDefault(int viewportW, int viewportH) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportW, viewportH);
}

}  // namespace render

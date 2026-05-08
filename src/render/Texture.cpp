#include "Texture.h"

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>

namespace render {

Texture::~Texture() {
    if (tex_) glDeleteTextures(1, &tex_);
}

bool Texture::createRGBA(int width, int height, const uint8_t* pixels, bool nearest) {
    if (tex_) glDeleteTextures(1, &tex_);

    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    GLint minF = nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    GLint magF = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magF);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    w_ = width;
    h_ = height;
    return true;
}

bool Texture::loadFromFile(const std::string& path, bool nearest) {
    int w, h, c;
    stbi_set_flip_vertically_on_load(true);
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data) {
        std::fprintf(stderr, "[texture] failed to load '%s'\n", path.c_str());
        return false;
    }
    bool ok = createRGBA(w, h, data, nearest);
    stbi_image_free(data);
    return ok;
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex_);
}

}  // namespace render

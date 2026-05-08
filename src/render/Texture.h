#pragma once

#include <cstdint>
#include <string>

namespace render {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // RGBA8 from raw bytes (size = width*height*4).
    bool createRGBA(int width, int height, const uint8_t* pixels, bool nearest = false);

    // Load from file (PNG/JPG/etc via stb_image).
    bool loadFromFile(const std::string& path, bool nearest = false);

    void bind(int unit = 0) const;
    unsigned int id() const { return tex_; }

private:
    unsigned int tex_ = 0;
    int          w_   = 0;
    int          h_   = 0;
};

}  // namespace render

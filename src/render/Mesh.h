#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace render {

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::ivec4 boneIds { 0 };
    glm::vec4  boneWeights { 0.0f };
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const std::vector<Vertex>& vertices,
                const std::vector<uint32_t>& indices);

    void draw() const;

    uint32_t indexCount() const { return indexCount_; }

private:
    void release();

    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    uint32_t     indexCount_ = 0;
};

}  // namespace render

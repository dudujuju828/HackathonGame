#include "Mesh.h"

#include <glad/gl.h>

#include <cstddef>
#include <utility>

namespace render {

Mesh::~Mesh() { release(); }

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), ebo_(other.ebo_),
      indexCount_(other.indexCount_) {
    other.vao_ = other.vbo_ = other.ebo_ = 0;
    other.indexCount_ = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        ebo_ = other.ebo_;
        indexCount_ = other.indexCount_;
        other.vao_ = other.vbo_ = other.ebo_ = 0;
        other.indexCount_ = 0;
    }
    return *this;
}

void Mesh::release() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    vao_ = vbo_ = ebo_ = 0;
    indexCount_ = 0;
}

void Mesh::upload(const std::vector<Vertex>& vertices,
                  const std::vector<uint32_t>& indices) {
    release();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));

    glBindVertexArray(0);
    indexCount_ = static_cast<uint32_t>(indices.size());
}

void Mesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

}  // namespace render

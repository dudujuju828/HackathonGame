#pragma once

#include "Mesh.h"
#include "Texture.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

namespace render {

// One renderable submesh within a Model.
// `diffuse` is null when the material had no map_Kd (or texture failed to load).
// Treat the model as opaque material data — the renderer decides how to bind it.
struct ModelMesh {
    Mesh                     mesh;
    std::shared_ptr<Texture> diffuse;
    glm::vec3                diffuseColor { 1.0f };
};

class Model {
public:
    Model() = default;

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) noexcept = default;
    Model& operator=(Model&&) noexcept = default;

    // Loads an OBJ (with optional MTL alongside it). Returns false on parse error.
    bool loadFromFile(const std::string& path);

    void clear() { meshes_.clear(); }

    const std::vector<ModelMesh>& meshes() const { return meshes_; }

private:
    std::vector<ModelMesh> meshes_;
};

}  // namespace render

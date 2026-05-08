#pragma once

#include "Animation.h"
#include "Mesh.h"
#include "Texture.h"

#include <glm/glm.hpp>

#include <memory>
#include <optional>
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

    // Dispatches to OBJ or glTF/GLB loader based on file extension.
    bool loadFromFile(const std::string& path);

    void clear() {
        meshes_.clear();
        skeleton_ = std::nullopt;
        animations_.clear();
    }

    const std::vector<ModelMesh>& meshes() const { return meshes_; }
    const std::optional<Skeleton>& skeleton() const { return skeleton_; }
    const std::vector<AnimationClip>& animations() const { return animations_; }

private:
    bool loadOBJ_(const std::string& path);
    bool loadGLB_(const std::string& path);

    std::vector<ModelMesh>     meshes_;
    std::optional<Skeleton>    skeleton_;
    std::vector<AnimationClip> animations_;
};

}  // namespace render

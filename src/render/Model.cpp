#include "Model.h"

#include <tiny_obj_loader.h>

#include <glm/geometric.hpp>

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <unordered_map>

namespace render {

namespace {

// (positionIdx, normalIdx, texcoordIdx) — uniquely identifies an OBJ vertex.
// We dedup these triplets into a flat indexed Mesh.
struct VertexKey {
    int p;
    int n;
    int t;
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& k) const noexcept {
        // FNV-ish mix; keys are dense small ints so collisions stay rare.
        std::size_t h = 1469598103934665603ull;
        h ^= static_cast<std::size_t>(k.p) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(k.n) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= static_cast<std::size_t>(k.t) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    }
};

struct VertexKeyEq {
    bool operator()(const VertexKey& a, const VertexKey& b) const noexcept {
        return a.p == b.p && a.n == b.n && a.t == b.t;
    }
};

glm::vec3 readPos(const tinyobj::attrib_t& a, int i) {
    return { a.vertices[3 * i + 0], a.vertices[3 * i + 1], a.vertices[3 * i + 2] };
}

glm::vec3 readNormal(const tinyobj::attrib_t& a, int i) {
    return { a.normals[3 * i + 0], a.normals[3 * i + 1], a.normals[3 * i + 2] };
}

glm::vec2 readUV(const tinyobj::attrib_t& a, int i) {
    return { a.texcoords[2 * i + 0], a.texcoords[2 * i + 1] };
}

}  // namespace

bool Model::loadFromFile(const std::string& path) {
    tinyobj::ObjReader        reader;
    tinyobj::ObjReaderConfig  cfg;

    std::filesystem::path objPath(path);
    cfg.mtl_search_path = objPath.parent_path().string();
    cfg.triangulate     = true;

    if (!reader.ParseFromFile(path, cfg)) {
        if (!reader.Error().empty()) {
            std::fprintf(stderr, "[model] parse error in '%s': %s\n",
                         path.c_str(), reader.Error().c_str());
        }
        return false;
    }
    if (!reader.Warning().empty()) {
        std::fprintf(stderr, "[model] %s", reader.Warning().c_str());
    }

    const tinyobj::attrib_t&                attrib    = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>&    shapes    = reader.GetShapes();
    const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

    meshes_.clear();

    // Texture cache scoped to this load — same map_Kd resolved once per Model.
    std::unordered_map<std::string, std::shared_ptr<Texture>> texCache;

    for (const tinyobj::shape_t& shape : shapes) {
        // Each material group within the shape becomes its own ModelMesh.
        struct Bucket {
            std::vector<Vertex>   verts;
            std::vector<uint32_t> indices;
            std::unordered_map<VertexKey, uint32_t, VertexKeyHash, VertexKeyEq> dedup;
        };
        std::unordered_map<int, Bucket> bucketsByMat;

        std::size_t indexOffset = 0;
        const std::size_t faceCount = shape.mesh.num_face_vertices.size();

        for (std::size_t f = 0; f < faceCount; ++f) {
            const int fv  = shape.mesh.num_face_vertices[f];   // expect 3 (triangulated)
            const int mat = shape.mesh.material_ids.empty()
                               ? -1
                               : shape.mesh.material_ids[f];

            // If any of this face's vertices lacks a normal, fall back to a flat
            // face normal so the lighting still works.
            glm::vec3 faceN(0.0f, 1.0f, 0.0f);
            bool needFaceN = false;
            for (int v = 0; v < fv; ++v) {
                if (shape.mesh.indices[indexOffset + v].normal_index < 0) {
                    needFaceN = true;
                    break;
                }
            }
            if (needFaceN && fv >= 3) {
                glm::vec3 p0 = readPos(attrib, shape.mesh.indices[indexOffset + 0].vertex_index);
                glm::vec3 p1 = readPos(attrib, shape.mesh.indices[indexOffset + 1].vertex_index);
                glm::vec3 p2 = readPos(attrib, shape.mesh.indices[indexOffset + 2].vertex_index);
                glm::vec3 n  = glm::cross(p1 - p0, p2 - p0);
                if (glm::length(n) > 1e-8f) faceN = glm::normalize(n);
            }

            Bucket& b = bucketsByMat[mat];

            for (int v = 0; v < fv; ++v) {
                const tinyobj::index_t i = shape.mesh.indices[indexOffset + v];
                const VertexKey key { i.vertex_index, i.normal_index, i.texcoord_index };

                auto it = b.dedup.find(key);
                uint32_t outIdx;
                if (it != b.dedup.end()) {
                    outIdx = it->second;
                } else {
                    Vertex vt{};
                    vt.pos    = readPos(attrib, i.vertex_index);
                    vt.normal = (i.normal_index   >= 0) ? readNormal(attrib, i.normal_index) : faceN;
                    vt.uv     = (i.texcoord_index >= 0) ? readUV(attrib, i.texcoord_index)   : glm::vec2(0.0f);
                    outIdx = static_cast<uint32_t>(b.verts.size());
                    b.verts.push_back(vt);
                    b.dedup.emplace(key, outIdx);
                }
                b.indices.push_back(outIdx);
            }
            indexOffset += fv;
        }

        // Emit one ModelMesh per material bucket.
        for (auto& [matId, b] : bucketsByMat) {
            if (b.verts.empty() || b.indices.empty()) continue;

            ModelMesh mm;
            mm.mesh.upload(b.verts, b.indices);

            if (matId >= 0 && matId < static_cast<int>(materials.size())) {
                const tinyobj::material_t& mtl = materials[matId];
                mm.diffuseColor = { mtl.diffuse[0], mtl.diffuse[1], mtl.diffuse[2] };

                if (!mtl.diffuse_texname.empty()) {
                    std::filesystem::path texFs = objPath.parent_path() / mtl.diffuse_texname;
                    std::string texPath = texFs.string();

                    auto cached = texCache.find(texPath);
                    if (cached != texCache.end()) {
                        mm.diffuse = cached->second;
                    } else {
                        auto tex = std::make_shared<Texture>();
                        if (tex->loadFromFile(texPath)) {
                            mm.diffuse = tex;
                        } else {
                            std::fprintf(stderr,
                                         "[model] missing texture '%s' (material '%s')\n",
                                         texPath.c_str(), mtl.name.c_str());
                        }
                        texCache.emplace(texPath, mm.diffuse);
                    }
                }
            }

            meshes_.push_back(std::move(mm));
        }
    }

    std::printf("[model] loaded '%s' (%zu submeshes)\n", path.c_str(), meshes_.size());
    return true;
}

}  // namespace render

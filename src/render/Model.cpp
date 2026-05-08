#include "Model.h"

#include <tiny_obj_loader.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <stb_image.h>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
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
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    if (ext == ".glb" || ext == ".gltf") return loadGLB_(path);
    return loadOBJ_(path);
}

bool Model::loadOBJ_(const std::string& path) {
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

namespace {

std::shared_ptr<Texture> loadGltfImage(const cgltf_image* img) {
    auto tex = std::make_shared<Texture>();
    if (!img) return tex;

    const uint8_t* bytes = nullptr;
    size_t         size  = 0;
    if (img->buffer_view) {
        const cgltf_buffer_view* bv = img->buffer_view;
        bytes = static_cast<const uint8_t*>(bv->buffer->data) + bv->offset;
        size  = bv->size;
    } else {
        return tex;  // External-URI images aren't supported (GLBs we use ship embedded).
    }

    // glTF uses top-left UV origin; Texture's stb path flips by default for
    // OBJ. Save/restore so we don't infect later loads.
    int w, h, c;
    stbi_set_flip_vertically_on_load(false);
    uint8_t* pixels = stbi_load_from_memory(bytes, static_cast<int>(size),
                                            &w, &h, &c, 4);
    stbi_set_flip_vertically_on_load(true);
    if (pixels) {
        tex->createRGBA(w, h, pixels, /*nearest=*/false);
        stbi_image_free(pixels);
    }
    return tex;
}

}  // namespace

bool Model::loadGLB_(const std::string& path) {
    cgltf_options opts {};
    cgltf_data*   data = nullptr;

    if (cgltf_parse_file(&opts, path.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "[model] cgltf_parse_file failed for '%s'\n", path.c_str());
        return false;
    }
    if (cgltf_load_buffers(&opts, data, path.c_str()) != cgltf_result_success) {
        std::fprintf(stderr, "[model] cgltf_load_buffers failed for '%s'\n", path.c_str());
        cgltf_free(data);
        return false;
    }

    meshes_.clear();
    std::unordered_map<const cgltf_image*, std::shared_ptr<Texture>> texCache;

    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
        const cgltf_mesh& mesh = data->meshes[mi];
        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi) {
            const cgltf_primitive& prim = mesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            const cgltf_accessor* posAcc  = nullptr;
            const cgltf_accessor* normAcc = nullptr;
            const cgltf_accessor* uvAcc   = nullptr;
            const cgltf_accessor* jointAcc = nullptr;
            const cgltf_accessor* weightAcc = nullptr;
            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                const cgltf_attribute& attr = prim.attributes[ai];
                switch (attr.type) {
                    case cgltf_attribute_type_position: posAcc  = attr.data; break;
                    case cgltf_attribute_type_normal:   normAcc = attr.data; break;
                    case cgltf_attribute_type_texcoord: if (!uvAcc) uvAcc = attr.data; break;
                    case cgltf_attribute_type_joints:   if (!jointAcc) jointAcc = attr.data; break;
                    case cgltf_attribute_type_weights:  if (!weightAcc) weightAcc = attr.data; break;
                    default: break;
                }
            }
            if (!posAcc) continue;

            const cgltf_size vCount = posAcc->count;
            std::vector<Vertex> verts(vCount);
            for (cgltf_size i = 0; i < vCount; ++i) {
                cgltf_accessor_read_float(posAcc, i, &verts[i].pos.x, 3);
                if (normAcc) {
                    cgltf_accessor_read_float(normAcc, i, &verts[i].normal.x, 3);
                } else {
                    verts[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
                if (uvAcc) {
                    cgltf_accessor_read_float(uvAcc, i, &verts[i].uv.x, 2);
                } else {
                    verts[i].uv = glm::vec2(0.0f);
                }
                if (jointAcc) {
                    cgltf_uint j[4] = {0,0,0,0};
                    cgltf_accessor_read_uint(jointAcc, i, j, 4);
                    verts[i].boneIds = glm::ivec4(j[0], j[1], j[2], j[3]);
                }
                if (weightAcc) {
                    cgltf_float w[4] = {0.0f};
                    cgltf_accessor_read_float(weightAcc, i, w, 4);
                    verts[i].boneWeights = glm::vec4(w[0], w[1], w[2], w[3]);
                }
            }

            std::vector<uint32_t> indices;
            if (prim.indices) {
                const cgltf_size iCount = prim.indices->count;
                indices.resize(iCount);
                for (cgltf_size i = 0; i < iCount; ++i) {
                    indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i));
                }
            } else {
                indices.resize(vCount);
                for (uint32_t i = 0; i < static_cast<uint32_t>(vCount); ++i) indices[i] = i;
            }

            // Generate flat normals if the mesh shipped without any.
            if (!normAcc) {
                for (size_t t = 0; t + 2 < indices.size(); t += 3) {
                    glm::vec3 p0 = verts[indices[t + 0]].pos;
                    glm::vec3 p1 = verts[indices[t + 1]].pos;
                    glm::vec3 p2 = verts[indices[t + 2]].pos;
                    glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
                    if (glm::length(n) > 1e-8f) n = glm::normalize(n);
                    verts[indices[t + 0]].normal = n;
                    verts[indices[t + 1]].normal = n;
                    verts[indices[t + 2]].normal = n;
                }
            }

            ModelMesh mm;
            mm.mesh.upload(verts, indices);

            if (prim.material && prim.material->has_pbr_metallic_roughness) {
                const auto& pbr = prim.material->pbr_metallic_roughness;
                mm.diffuseColor = glm::vec3(pbr.base_color_factor[0],
                                            pbr.base_color_factor[1],
                                            pbr.base_color_factor[2]);
                if (pbr.base_color_texture.texture &&
                    pbr.base_color_texture.texture->image) {
                    const cgltf_image* img = pbr.base_color_texture.texture->image;
                    auto it = texCache.find(img);
                    if (it != texCache.end()) {
                        mm.diffuse = it->second;
                    } else {
                        auto tex = loadGltfImage(img);
                        mm.diffuse = tex;
                        texCache.emplace(img, tex);
                    }
                }
            }

            meshes_.push_back(std::move(mm));
        }
    }

    // Parse Skeleton
    skeleton_ = std::nullopt;
    std::unordered_map<const cgltf_node*, int> nodeToJoint;
    if (data->skins_count > 0) {
        const cgltf_skin& skin = data->skins[0];
        Skeleton skel;
        skel.joints.resize(skin.joints_count);

        for (cgltf_size i = 0; i < skin.joints_count; ++i) {
            nodeToJoint[skin.joints[i]] = static_cast<int>(i);
        }

        for (cgltf_size i = 0; i < skin.joints_count; ++i) {
            const cgltf_node* node = skin.joints[i];
            Joint& j = skel.joints[i];
            if (node->name) j.name = node->name;

            if (node->parent && nodeToJoint.find(node->parent) != nodeToJoint.end()) {
                j.parentIndex = nodeToJoint[node->parent];
            } else {
                j.parentIndex = -1;
            }

            if (skin.inverse_bind_matrices) {
                cgltf_accessor_read_float(skin.inverse_bind_matrices, i, glm::value_ptr(j.inverseBindMatrix), 16);
            }

            if (node->has_translation) {
                j.translation = glm::vec3(node->translation[0], node->translation[1], node->translation[2]);
            }
            if (node->has_rotation) {
                j.rotation = glm::quat(node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
            }
            if (node->has_scale) {
                j.scale = glm::vec3(node->scale[0], node->scale[1], node->scale[2]);
            }
        }
        skeleton_ = std::move(skel);
    }

    // Parse Animations
    animations_.clear();
    for (cgltf_size a = 0; a < data->animations_count; ++a) {
        const cgltf_animation& anim = data->animations[a];
        AnimationClip clip;
        if (anim.name) clip.name = anim.name;

        float maxTime = 0.0f;

        for (cgltf_size c = 0; c < anim.channels_count; ++c) {
            const cgltf_animation_channel& chan = anim.channels[c];
            if (!chan.target_node) continue;

            auto it = nodeToJoint.find(chan.target_node);
            if (it == nodeToJoint.end()) continue;

            int jointIdx = it->second;

            AnimationChannel* ourChan = nullptr;
            for (auto& ch : clip.channels) {
                if (ch.targetJoint == jointIdx) {
                    ourChan = &ch; break;
                }
            }
            if (!ourChan) {
                clip.channels.push_back({jointIdx, {}, {}, {}});
                ourChan = &clip.channels.back();
            }

            const cgltf_animation_sampler* samp = chan.sampler;
            if (!samp || !samp->input || !samp->output) continue;

            const cgltf_accessor* timeAcc = samp->input;
            const cgltf_accessor* valAcc = samp->output;

            for (cgltf_size k = 0; k < timeAcc->count; ++k) {
                float t = 0.0f;
                cgltf_accessor_read_float(timeAcc, k, &t, 1);
                maxTime = std::max(maxTime, t);

                if (chan.target_path == cgltf_animation_path_type_translation) {
                    glm::vec3 v;
                    cgltf_accessor_read_float(valAcc, k, glm::value_ptr(v), 3);
                    ourChan->translations.push_back({t, v});
                } else if (chan.target_path == cgltf_animation_path_type_rotation) {
                    float v[4];
                    cgltf_accessor_read_float(valAcc, k, v, 4);
                    // cgltf rotation is [x, y, z, w], glm::quat is (w, x, y, z)
                    ourChan->rotations.push_back({t, glm::quat(v[3], v[0], v[1], v[2])});
                } else if (chan.target_path == cgltf_animation_path_type_scale) {
                    glm::vec3 v;
                    cgltf_accessor_read_float(valAcc, k, glm::value_ptr(v), 3);
                    ourChan->scales.push_back({t, v});
                }
            }
        }
        clip.duration = maxTime;
        animations_.push_back(std::move(clip));
    }

    cgltf_free(data);
    std::printf("[model] loaded GLB '%s' (%zu submeshes)\n",
                path.c_str(), meshes_.size());
    return true;
}

}  // namespace render

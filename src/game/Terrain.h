#pragma once

#include "render/Mesh.h"
#include "render/Texture.h"

#include <glm/glm.hpp>
#include <memory>

namespace game {

enum class Zone { Default, Forest, Graveyard, Asylum };

struct ZoneInfo {
    Zone        zone;
    const char* name;
    float       xMin, xMax, zMin, zMax;
    glm::vec3   tint;  // color tint hint for rendering in that zone
};

class Terrain {
public:
    static constexpr int   kGrid      = 129;    // vertices per side (128 quads)
    static constexpr float kWorldSize = 128.0f; // total extent in meters
    static constexpr float kMaxHeight = 2.0f;   // peak height deviation from 0

    void generate(int mapIndex = 0);

    float    heightAt(float x, float z) const; // bilinear-interpolated world height
    Zone     zoneAt(float x, float z) const;
    ZoneInfo zoneInfoAt(float x, float z) const;

    const render::Mesh&    mesh()    const { return mesh_; }
    const render::Texture& texture() const { return *texture_; }

private:
    float heights_[kGrid][kGrid]{};

    render::Mesh                     mesh_;
    std::shared_ptr<render::Texture> texture_;

    static const ZoneInfo kZones[4];

    static float perlin(float x, float y);
    void buildMesh();
    void buildTexture();
};

} // namespace game

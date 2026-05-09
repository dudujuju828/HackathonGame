#include "Terrain.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace game {

// Per-quadrant biome noise character. The four corners of the map blend
// smoothly into each other via bilinear weights, so the player walks
// through one continuous terrain whose feel changes with location:
//
//   NW (Default):   rolling hills     — moderate amplitude, balanced octaves
//   NE (Asylum):    dramatic ridges   — high amplitude, low-freq dominant
//   SW (Forest):    dense rolling     — high amplitude, mid-freq prominent
//   SE (Graveyard): flat-ish + bumps  — low amplitude, surface detail dominant
struct BiomeDef {
    float offX, offZ;
    float amplitude;
    float f1, w1;
    float f2, w2;
    float f3, w3;
};

static constexpr BiomeDef kBiomes[4] = {
    // SW — Forest: dense rolling hills
    {  10.0f,  10.0f, 14.0f,  0.008f, 0.75f,  0.030f, 0.20f,  0.100f, 0.05f },
    // SE — Graveyard: gentle undulating with surface texture
    { 120.0f,  30.0f,  6.0f,  0.006f, 0.55f,  0.025f, 0.30f,  0.090f, 0.15f },
    // NW — Default: classic rolling hills
    {  60.0f, 200.0f, 12.0f,  0.009f, 0.65f,  0.032f, 0.25f,  0.100f, 0.10f },
    // NE — Asylum: dramatic highlands and ridges
    {  50.0f,  75.0f, 22.0f,  0.005f, 0.82f,  0.020f, 0.14f,  0.080f, 0.04f },
};

static float smoothstepF(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Four quadrants of the 128x128 map.
const ZoneInfo Terrain::kZones[4] = {
    { Zone::Default,   "Default",   -64.0f,  0.0f,   0.0f, 64.0f, { 1.0f, 1.0f, 1.0f } },
    { Zone::Forest,    "Forest",    -64.0f,  0.0f, -64.0f,  0.0f, { 0.6f, 1.2f, 0.6f } },
    { Zone::Graveyard, "Graveyard",  0.0f,  64.0f, -64.0f,  0.0f, { 0.9f, 0.9f, 1.1f } },
    { Zone::Asylum,    "Asylum",     0.0f,  64.0f,   0.0f, 64.0f, { 1.1f, 0.8f, 0.8f } },
};

// Ken Perlin's reference permutation table repeated for wraparound.
static constexpr int kP[512] = {
    151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
    140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
    247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
     57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
     74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
     60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
     65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
    200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
     52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
    207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
    119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
    129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
    218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
     81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
    184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
    222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180,
    // repeated
    151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
    140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
    247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
     57,177, 33, 88,237,149, 56, 87,174, 20,125,136,171,168, 68,175,
     74,165, 71,134,139, 48, 27,166, 77,146,158,231, 83,111,229,122,
     60,211,133,230,220,105, 92, 41, 55, 46,245, 40,244,102,143, 54,
     65, 25, 63,161,  1,216, 80, 73,209, 76,132,187,208, 89, 18,169,
    200,196,135,130,116,188,159, 86,164,100,109,198,173,186,  3, 64,
     52,217,226,250,124,123,  5,202, 38,147,118,126,255, 82, 85,212,
    207,206, 59,227, 47, 16, 58, 17,182,189, 28, 42,223,183,170,213,
    119,248,152,  2, 44,154,163, 70,221,153,101,155,167, 43,172,  9,
    129, 22, 39,253, 19, 98,108,110, 79,113,224,232,178,185,112,104,
    218,246, 97,228,251, 34,242,193,238,210,144, 12,191,179,162,241,
     81, 51,145,235,249, 14,239,107, 49,192,214, 31,181,199,106,157,
    184, 84,204,176,115,121, 50, 45,127,  4,150,254,138,236,205, 93,
    222,114, 67, 29, 24, 72,243,141,128,195, 78, 66,215, 61,156,180,
};

static float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float grad2(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float Terrain::perlin(float x, float y) {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    x -= std::floor(x);
    y -= std::floor(y);
    float u = fade(x);
    float v = fade(y);
    int a = kP[X]     + Y;
    int b = kP[X + 1] + Y;
    return glm::mix(
        glm::mix(grad2(kP[a],     x,        y),
                 grad2(kP[b],     x - 1.0f, y),         u),
        glm::mix(grad2(kP[a + 1], x,        y - 1.0f),
                 grad2(kP[b + 1], x - 1.0f, y - 1.0f), u),
        v);
}

void Terrain::generate(int /*mapIndex*/) {
    // Single unified terrain. Each vertex samples all four biome param sets
    // and blends them with bilinear weights based on world position, so the
    // four corners blend smoothly into one continuous landscape.
    const float step = kWorldSize / static_cast<float>(kGrid - 1);
    const float half = kWorldSize * 0.5f;

    for (int i = 0; i < kGrid; ++i) {
        for (int j = 0; j < kGrid; ++j) {
            const float wx = -half + static_cast<float>(i) * step;
            const float wz = -half + static_cast<float>(j) * step;

            // Bilinear weights across the world (0..1 east-west and south-north).
            const float u = smoothstepF((wx + half) / kWorldSize);
            const float v = smoothstepF((wz + half) / kWorldSize);

            const float w_sw = (1.0f - u) * (1.0f - v);  // index 0 — Forest
            const float w_se =         u  * (1.0f - v);  // index 1 — Graveyard
            const float w_nw = (1.0f - u) *         v;   // index 2 — Default
            const float w_ne =         u  *         v;   // index 3 — Asylum

            float h = 0.0f;
            const float weights[4] = { w_sw, w_se, w_nw, w_ne };
            for (int b = 0; b < 4; ++b) {
                if (weights[b] < 1e-4f) continue;
                const BiomeDef& m = kBiomes[b];
                const float bh = m.amplitude * (
                    m.w1 * perlin(wx * m.f1 + m.offX, wz * m.f1 + m.offZ) +
                    m.w2 * perlin(wx * m.f2 + m.offX, wz * m.f2 + m.offZ) +
                    m.w3 * perlin(wx * m.f3 + m.offX, wz * m.f3 + m.offZ));
                h += weights[b] * bh;
            }
            heights_[i][j] = h;
        }
    }

    buildMesh();
    buildTexture();
}

float Terrain::heightAt(float x, float z) const {
    const float step = kWorldSize / static_cast<float>(kGrid - 1);
    const float half = kWorldSize * 0.5f;

    float gx = (x + half) / step;
    float gz = (z + half) / step;

    const float maxIdx = static_cast<float>(kGrid - 1) - 0.001f;
    gx = std::max(0.0f, std::min(gx, maxIdx));
    gz = std::max(0.0f, std::min(gz, maxIdx));

    int ix = static_cast<int>(gx);
    int iz = static_cast<int>(gz);
    float fx = gx - static_cast<float>(ix);
    float fz = gz - static_cast<float>(iz);

    float h00 = heights_[ix    ][iz    ];
    float h10 = heights_[ix + 1][iz    ];
    float h01 = heights_[ix    ][iz + 1];
    float h11 = heights_[ix + 1][iz + 1];

    return glm::mix(glm::mix(h00, h10, fx), glm::mix(h01, h11, fx), fz);
}

Zone Terrain::zoneAt(float x, float z) const {
    return zoneInfoAt(x, z).zone;
}

ZoneInfo Terrain::zoneInfoAt(float x, float z) const {
    for (const auto& zone : kZones) {
        if (x >= zone.xMin && x < zone.xMax &&
            z >= zone.zMin && z < zone.zMax) {
            return zone;
        }
    }
    return kZones[0];
}

void Terrain::buildMesh() {
    const float step = kWorldSize / static_cast<float>(kGrid - 1);
    const float half = kWorldSize * 0.5f;
    // Tile the ground texture every 4 metres.
    const float uvScale = 1.0f / 4.0f;

    std::vector<render::Vertex> verts;
    verts.reserve(static_cast<size_t>(kGrid) * kGrid);

    for (int i = 0; i < kGrid; ++i) {
        for (int j = 0; j < kGrid; ++j) {
            float wx = -half + static_cast<float>(i) * step;
            float wz = -half + static_cast<float>(j) * step;
            float wy =  heights_[i][j];

            // Smooth normals via central (or one-sided at boundary) finite differences.
            float dhdx, dhdz;
            if      (i == 0)        dhdx = (heights_[1][j]         - heights_[0][j])         / step;
            else if (i == kGrid-1)  dhdx = (heights_[kGrid-1][j]   - heights_[kGrid-2][j])   / step;
            else                    dhdx = (heights_[i+1][j]        - heights_[i-1][j])       / (2.0f * step);

            if      (j == 0)        dhdz = (heights_[i][1]         - heights_[i][0])         / step;
            else if (j == kGrid-1)  dhdz = (heights_[i][kGrid-1]   - heights_[i][kGrid-2])   / step;
            else                    dhdz = (heights_[i][j+1]        - heights_[i][j-1])       / (2.0f * step);

            glm::vec3 normal = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
            glm::vec2 uv     = glm::vec2(wx * uvScale, wz * uvScale);

            verts.push_back({ glm::vec3(wx, wy, wz), normal, uv });
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(kGrid - 1) * (kGrid - 1) * 6);

    for (int i = 0; i < kGrid - 1; ++i) {
        for (int j = 0; j < kGrid - 1; ++j) {
            // Vertex indices for the four corners of this quad.
            // i = X axis, j = Z axis; vertex index = i*kGrid + j.
            uint32_t a = static_cast<uint32_t>( i      * kGrid +  j);       // (i,   j  )
            uint32_t b = static_cast<uint32_t>((i + 1) * kGrid +  j);       // (i+1, j  )
            uint32_t c = static_cast<uint32_t>( i      * kGrid + (j + 1));  // (i,   j+1)
            uint32_t d = static_cast<uint32_t>((i + 1) * kGrid + (j + 1)); // (i+1, j+1)

            // CCW winding so normals face +Y (verified via cross product).
            indices.insert(indices.end(), { a, d, b });
            indices.insert(indices.end(), { a, c, d });
        }
    }

    mesh_.upload(verts, indices);
}

void Terrain::buildTexture() {
    const int size = 64;
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // Earthy ground color with per-pixel hash variation.
            int hash = (x * 1619 + y * 31337) & 0xFF;
            auto px = [&](size_t idx) -> uint8_t& { return pixels[idx]; };
            size_t i = (static_cast<size_t>(y) * size + x) * 4;
            px(i+0) = static_cast<uint8_t>(42 + (hash & 0x0F));        // R
            px(i+1) = static_cast<uint8_t>(60 + ((hash >> 2) & 0x0F)); // G
            px(i+2) = static_cast<uint8_t>(28 + (hash & 0x07));        // B
            px(i+3) = 255;
        }
    }
    texture_ = std::make_shared<render::Texture>();
    texture_->createRGBA(size, size, pixels.data(), /*nearest=*/true);
}

} // namespace game

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward-declare miniaudio types to keep the header free of the (large)
// miniaudio.h. The actual structs are heap-allocated inside the .cpp.
typedef struct ma_engine ma_engine;
typedef struct ma_sound  ma_sound;

namespace audio {

// Thin wrapper around miniaudio's ma_engine + a name-keyed cache of
// ma_sound instances. Move-only RAII; one instance per game session,
// owned by main and passed to systems that need to trigger sounds.
class Audio {
public:
    Audio();
    ~Audio();

    Audio(const Audio&)            = delete;
    Audio& operator=(const Audio&) = delete;

    bool init();
    void shutdown();
    bool ready() const { return inited_; }

    // Decodes the file fully into memory and caches it under `name`. Use
    // looping=true for ambient/footstep loops; the looping flag is held
    // on the cached instance so subsequent start()s respect it.
    bool loadSound(const std::string& name, const std::string& path, bool looping = false);

    // Idempotent start (safe to call every frame for a loop); play() rewinds
    // before starting and is intended for one-shots.
    void start(const std::string& name);
    void play (const std::string& name);
    void stop (const std::string& name);

    bool isPlaying(const std::string& name) const;

    // Fire-and-forget one-shot. Spins up a transient voice that decodes via
    // miniaudio's resource manager (path-cached after first call), so it's
    // safe to spam from gameplay code without cutting prior plays off.
    void playOneShot(const std::string& path, float volume = 1.0f);

    // 3D listener pose. Call once per frame with the camera's position,
    // forward direction, and world-up. xyz triples are floats in metres /
    // unit vectors so the header stays glm-free.
    void setListener(const float pos[3], const float fwd[3], const float up[3]);

    // Spatialised one-shot. Volume is a per-source pre-attenuation gain;
    // distance falloff is applied automatically using the engine's linear
    // model with min=2 m, max=28 m. Sounds finished playing are reaped on
    // subsequent calls so the active list doesn't grow unbounded.
    void playPositional(const std::string& path, const float pos[3], float volume = 1.0f);

    void setVolume(const std::string& name, float v);
    void setPitch (const std::string& name, float p);

    // 0..1 engine-wide gain. Multiplied with each sound's own volume.
    void setMasterVolume(float v);

private:
    void pruneFinishedOneShots_();

    ma_engine* engine_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<ma_sound>> sounds_;
    // Active spatialised one-shots; each owns its own ma_sound and is
    // reaped once playback finishes (see pruneFinishedOneShots_).
    std::vector<ma_sound*> oneShots_;
    bool inited_ = false;
};

}  // namespace audio

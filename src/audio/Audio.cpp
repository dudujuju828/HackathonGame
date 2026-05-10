// miniaudio is single-header; this is the only TU that defines the
// implementation macro. Wrap the include to silence the compiler
// warnings the project enables (W4 / Wpedantic) — miniaudio is
// production-tested but doesn't compile clean against strict flags.
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wall"
#  pragma GCC diagnostic ignored "-Wextra"
#  pragma GCC diagnostic ignored "-Wpedantic"
#  pragma GCC diagnostic ignored "-Wunused-function"
#  pragma GCC diagnostic ignored "-Wunused-variable"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wsign-compare"
#  pragma GCC diagnostic ignored "-Wtype-limits"
#elif defined(_MSC_VER)
#  pragma warning(push, 0)
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#  pragma warning(pop)
#endif

#include "Audio.h"

#include <algorithm>
#include <cstdio>

namespace audio {

Audio::Audio() = default;

Audio::~Audio() {
    shutdown();
}

bool Audio::init() {
    if (inited_) return true;
    engine_ = new ma_engine{};
    const ma_result r = ma_engine_init(nullptr, engine_);
    if (r != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] ma_engine_init failed (%d)\n", static_cast<int>(r));
        delete engine_;
        engine_ = nullptr;
        return false;
    }
    inited_ = true;
    std::printf("[audio] engine ready\n");
    return true;
}

void Audio::shutdown() {
    if (!inited_) return;
    for (auto& [name, snd] : sounds_) {
        if (snd) ma_sound_uninit(snd.get());
    }
    sounds_.clear();
    for (ma_sound* s : oneShots_) {
        if (!s) continue;
        ma_sound_uninit(s);
        delete s;
    }
    oneShots_.clear();
    if (engine_) {
        ma_engine_uninit(engine_);
        delete engine_;
        engine_ = nullptr;
    }
    inited_ = false;
}

bool Audio::loadSound(const std::string& name, const std::string& path, bool looping) {
    if (!inited_) return false;

    auto snd = std::make_unique<ma_sound>();
    const ma_uint32 flags = MA_SOUND_FLAG_DECODE;  // decode fully on load
    const ma_result r = ma_sound_init_from_file(engine_, path.c_str(), flags,
                                                nullptr, nullptr, snd.get());
    if (r != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] failed to load '%s' (%d)\n",
                     path.c_str(), static_cast<int>(r));
        return false;
    }
    ma_sound_set_looping(snd.get(), looping ? MA_TRUE : MA_FALSE);
    // Cached named sounds are non-spatial by default — they're for UI /
    // ambience / loops where the listener pose mustn't affect mix.
    // playPositional() opts in to spatialisation per call.
    ma_sound_set_spatialization_enabled(snd.get(), MA_FALSE);
    sounds_[name] = std::move(snd);
    return true;
}

void Audio::start(const std::string& name) {
    auto it = sounds_.find(name);
    if (it == sounds_.end()) return;
    if (ma_sound_is_playing(it->second.get())) return;
    ma_sound_start(it->second.get());
}

void Audio::play(const std::string& name) {
    auto it = sounds_.find(name);
    if (it == sounds_.end()) return;
    ma_sound_seek_to_pcm_frame(it->second.get(), 0);
    ma_sound_start(it->second.get());
}

void Audio::stop(const std::string& name) {
    auto it = sounds_.find(name);
    if (it == sounds_.end()) return;
    ma_sound_stop(it->second.get());
}

bool Audio::isPlaying(const std::string& name) const {
    auto it = sounds_.find(name);
    if (it == sounds_.end()) return false;
    return ma_sound_is_playing(it->second.get()) == MA_TRUE;
}

void Audio::setVolume(const std::string& name, float v) {
    auto it = sounds_.find(name);
    if (it == sounds_.end()) return;
    ma_sound_set_volume(it->second.get(), v);
}

void Audio::setPitch(const std::string& name, float p) {
    auto it = sounds_.find(name);
    if (it == sounds_.end()) return;
    if (p < 0.05f) p = 0.05f;  // miniaudio rejects 0/negative pitch
    ma_sound_set_pitch(it->second.get(), p);
}

void Audio::setMasterVolume(float v) {
    if (engine_) ma_engine_set_volume(engine_, v);
}

void Audio::playOneShot(const std::string& path, float /*volume*/) {
    if (!inited_ || !engine_) return;
    // ma_engine_play_sound uses the engine's internal resource manager —
    // first call decodes from disk, subsequent calls reuse the cached
    // PCM. Polyphonic by construction; each call gets its own voice and
    // is auto-released when it finishes.
    ma_engine_play_sound(engine_, path.c_str(), nullptr);
}

void Audio::setListener(const float pos[3], const float fwd[3], const float up[3]) {
    if (!engine_) return;
    ma_engine_listener_set_position (engine_, 0, pos[0], pos[1], pos[2]);
    ma_engine_listener_set_direction(engine_, 0, fwd[0], fwd[1], fwd[2]);
    ma_engine_listener_set_world_up (engine_, 0, up[0],  up[1],  up[2]);
}

void Audio::pruneFinishedOneShots_() {
    oneShots_.erase(
        std::remove_if(oneShots_.begin(), oneShots_.end(),
            [](ma_sound* s) {
                if (!s) return true;
                if (ma_sound_is_playing(s)) return false;
                ma_sound_uninit(s);
                delete s;
                return true;
            }),
        oneShots_.end());
}

void Audio::playPositional(const std::string& path, const float pos[3], float volume) {
    if (!inited_ || !engine_) return;
    pruneFinishedOneShots_();

    auto* snd = new ma_sound{};
    if (ma_sound_init_from_file(engine_, path.c_str(),
                                MA_SOUND_FLAG_DECODE,
                                nullptr, nullptr, snd) != MA_SUCCESS) {
        delete snd;
        return;
    }
    ma_sound_set_volume(snd, volume);
    ma_sound_set_spatialization_enabled(snd, MA_TRUE);
    ma_sound_set_attenuation_model(snd, ma_attenuation_model_linear);
    ma_sound_set_min_distance(snd, 2.0f);
    ma_sound_set_max_distance(snd, 28.0f);
    ma_sound_set_position(snd, pos[0], pos[1], pos[2]);
    ma_sound_start(snd);
    oneShots_.push_back(snd);
}

}  // namespace audio

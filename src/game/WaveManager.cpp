#include "WaveManager.h"

#include "EnemySpawner.h"

#include <cstdio>

namespace game {

namespace {
const std::string kEmptyName{};
const std::string kAllClear{ "ALL CLEAR" };
}

void WaveManager::init(std::vector<WaveDef> waves, EnemySpawner* spawner) {
    waves_   = std::move(waves);
    spawner_ = spawner;
    reset();
}

void WaveManager::reset() {
    currentWave_       = 0;
    waveElapsed_       = 0.0f;
    finishedAll_       = waves_.empty();
    antidoteCollected_ = false;
    resetGroupFlags_();
}

void WaveManager::resetGroupFlags_() {
    groupSpawned_.clear();
    if (currentWave_ >= 0 && currentWave_ < static_cast<int>(waves_.size())) {
        groupSpawned_.assign(waves_[currentWave_].groups.size(), false);
    }
}

const std::string& WaveManager::currentWaveName() const {
    if (finishedAll_) return kAllClear;
    if (currentWave_ < 0 || currentWave_ >= static_cast<int>(waves_.size())) return kEmptyName;
    return waves_[currentWave_].name;
}

float WaveManager::waveTimeRemaining() const {
    if (finishedAll_ || currentWave_ >= static_cast<int>(waves_.size())) return 0.0f;
    const float d = waves_[currentWave_].duration;
    return (d > waveElapsed_) ? (d - waveElapsed_) : 0.0f;
}

void WaveManager::update(float dt, std::vector<Enemy>& enemies, const glm::vec3& playerPos) {
    if (finishedAll_ || !spawner_) return;
    if (currentWave_ >= static_cast<int>(waves_.size())) {
        finishedAll_ = true;
        return;
    }

    waveElapsed_ += dt;
    const WaveDef& w = waves_[currentWave_];

    // Spawn any groups whose `at` time has been reached this frame.
    for (size_t gi = 0; gi < w.groups.size(); ++gi) {
        if (groupSpawned_[gi]) continue;
        if (waveElapsed_ < w.groups[gi].at) continue;
        groupSpawned_[gi] = true;
        for (const auto& entry : w.groups[gi].entries) {
            const int idx = spawner_->findDefIndex(entry.name);
            if (idx < 0) {
                std::fprintf(stderr, "[wave] unknown enemy '%s' (wave %d)\n",
                             entry.name.c_str(), currentWave_ + 1);
                continue;
            }
            for (int n = 0; n < entry.count; ++n) {
                spawner_->spawnAtIndex(enemies, spawner_->ringPositionAround(playerPos), idx);
            }
        }
    }

    // Wave's duration is up AND the antidote box has been collected — advance.
    if (waveElapsed_ >= w.duration && antidoteCollected_) {
        currentWave_++;
        waveElapsed_       = 0.0f;
        antidoteCollected_ = false;
        if (currentWave_ >= static_cast<int>(waves_.size())) {
            finishedAll_ = true;
        } else {
            resetGroupFlags_();
        }
    }
}

bool WaveManager::waitingForAntidote() const {
    if (finishedAll_) return false;
    if (currentWave_ >= static_cast<int>(waves_.size())) return false;
    return waveElapsed_ >= waves_[currentWave_].duration && !antidoteCollected_;
}

}  // namespace game

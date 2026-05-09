#pragma once

#include "Enemy.h"
#include "Wave.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace game {

class EnemySpawner;

// Drives wave progression: ticks the current wave's timer, fires its
// scheduled groups against the spawner, advances to the next wave when
// the duration elapses. After the last wave, holds at "ALL CLEAR".
class WaveManager {
public:
    void init(std::vector<WaveDef> waves, EnemySpawner* spawner);

    void update(float dt, std::vector<Enemy>& enemies, const glm::vec3& playerPos);

    bool finished() const { return finishedAll_; }
    int  currentWaveIndex() const { return currentWave_; }
    int  totalWaves() const { return static_cast<int>(waves_.size()); }
    const std::string& currentWaveName() const;
    float waveTimeRemaining() const;
    float waveElapsed() const { return waveElapsed_; }

    // Reset back to wave 0; called when the player swaps maps or restarts.
    void reset();

private:
    std::vector<WaveDef> waves_;
    EnemySpawner*        spawner_     = nullptr;
    int                  currentWave_ = 0;
    float                waveElapsed_ = 0.0f;
    std::vector<bool>    groupSpawned_;
    bool                 finishedAll_ = false;

    void resetGroupFlags_();
};

}  // namespace game

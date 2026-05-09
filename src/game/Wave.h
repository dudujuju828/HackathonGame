#pragma once

#include <string>
#include <vector>

namespace game {

// One scheduled batch of enemies inside a wave. `at` is seconds relative
// to wave start. `entries` is a list of (enemy-name, count) pairs that
// the WaveManager resolves to def indices via EnemySpawner::findDefIndex.
struct WaveGroup {
    float at = 0.0f;
    struct Entry { std::string name; int count = 0; };
    std::vector<Entry> entries;
};

// One wave. duration is the total wall-clock seconds the wave lasts —
// once elapsed, no more groups spawn even if some haven't fired yet.
struct WaveDef {
    std::string            name;
    float                  duration = 30.0f;
    std::vector<WaveGroup> groups;
};

// Parse a waves config file. Returns true on success. Format:
//   # comment lines and blank lines are ignored
//   WAVE "Display Name" duration_sec
//   GROUP at_sec  Name count [Name count ...]
// Multiple WAVE blocks may follow each other.
bool loadWaves(const std::string& path, std::vector<WaveDef>& out);

}  // namespace game

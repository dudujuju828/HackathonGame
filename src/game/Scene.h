#pragma once

namespace game {

enum class Scene {
    Playing,
    Settings,
    LevelUp,
    Inventory,
    Loot,       // chest opened, popup awaiting click-to-continue
};

}  // namespace game

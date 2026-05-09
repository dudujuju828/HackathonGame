#pragma once

namespace game {

enum class Scene {
    StartMenu,  // initial briefing screen before gameplay begins
    Playing,
    Settings,
    LevelUp,
    Inventory,
    Journal,    // field journal — species/disease reference, opened with J
    Loot,       // chest opened, popup awaiting click-to-continue
    GameOver,   // player died — shows restart screen
};

}  // namespace game

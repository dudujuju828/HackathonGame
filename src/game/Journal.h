#pragma once

#include <string>
#include <vector>

namespace core  { class Input; }
namespace render { class Hud; class Text; }

namespace game {

struct JournalEntry {
    std::string species;
    std::string disease;
    std::string description;
    std::string symptoms;
    std::string treatment;
};

class JournalScreen {
public:
    bool load(const std::string& csvPath);

    // Reset to page 0 and clear input state. Call when opening the journal.
    void open();

    // Unlock the next journal entry and start the toast notification.
    void unlock();

    // Tick the toast timer down. Call every frame during Playing scene.
    void tickToast(float dt);

    // Reset unlocked progress back to 0. Call on game restart / map swap.
    void resetProgress();

    // Handle arrow-key and mouse-click navigation. Call each frame while open.
    void update(const core::Input& input, int W, int H);

    void draw(render::Hud& hud, render::Text& text, int W, int H) const;

    // Draw the non-pausing toast overlay. Call every frame during Playing scene.
    void drawToast(render::Hud& hud, render::Text& text, int W, int H) const;

    bool empty()     const { return entries_.empty(); }
    int  pageCount() const { return static_cast<int>(entries_.size()); }

private:
    static std::vector<std::string> wrapText(const std::string& s, float maxPx, float scale);

    std::vector<JournalEntry> entries_;
    int         page_          = 0;
    int         unlockedCount_ = 0;
    float       toastTimer_    = 0.0f;
    std::string toastDisease_;
    bool prevLeft_   = false;
    bool prevRight_  = false;
    bool prevClick_  = false;
};

} // namespace game

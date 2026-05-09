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

    // Handle arrow-key and mouse-click navigation. Call each frame while open.
    void update(const core::Input& input, int W, int H);

    void draw(render::Hud& hud, render::Text& text, int W, int H) const;

    bool empty()     const { return entries_.empty(); }
    int  pageCount() const { return static_cast<int>(entries_.size()); }

private:
    static std::vector<std::string> wrapText(const std::string& s, float maxPx, float scale);

    std::vector<JournalEntry> entries_;
    int  page_       = 0;
    bool prevLeft_   = false;
    bool prevRight_  = false;
    bool prevClick_  = false;
};

} // namespace game

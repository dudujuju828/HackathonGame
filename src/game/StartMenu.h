#pragma once

#include <string>
#include <vector>

namespace core  { class Input; }
namespace render { class Hud; class Text; }

namespace game {

class StartMenu {
public:
    bool load(const std::string& path);

    // Returns true the frame the player clicks Start Mission.
    bool update(const core::Input& input, int W, int H);

    void draw(render::Hud& hud, render::Text& text, int W, int H) const;

private:
    struct Line {
        std::string text;
        bool        isLabel = false; // true for section headers (ends with ':')
        bool        isEmpty = false; // blank line -> vertical spacer
    };

    static std::vector<std::string> wrapText(const std::string& s, float maxPx, float scale);

    std::vector<Line> lines_;
    bool prevClick_ = false;
    bool hovering_  = false;
};

} // namespace game

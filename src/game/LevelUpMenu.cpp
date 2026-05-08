#include "LevelUpMenu.h"

#include "core/Input.h"
#include "render/Hud.h"
#include "render/Text.h"

#include <GLFW/glfw3.h>
#include <algorithm>

namespace game {

namespace {
constexpr float kOptionW      = 300.0f;
constexpr float kOptionH      = 400.0f;
constexpr float kOptionSpacing = 40.0f;
}

float LevelUpMenu::rand01_() {
    rngState_ = rngState_ * 1664525u + 1013904223u;
    return static_cast<float>(rngState_ >> 8) / static_cast<float>(1u << 24);
}

void LevelUpMenu::reset() {
    auto pool = getUpgradePool();
    options_.clear();
    blockUntilRelease_ = true;
    ignoreTimer_ = 0.4f; // 400ms delay to prevent accidental/synthetic clicks
    
    // Pick 3 unique random upgrades if possible.
    for (int i = 0; i < 3 && !pool.empty(); ++i) {
        int idx = static_cast<int>(rand01_() * pool.size());
        options_.push_back(pool[idx]);
        pool.erase(pool.begin() + idx);
    }
}

LevelUpMenu::Rect LevelUpMenu::optionRect_(int W, int H, int slotIdx) const {
    const float totalW = 3.0f * kOptionW + 2.0f * kOptionSpacing;
    const float startX = (W - totalW) * 0.5f;
    const float x0 = startX + slotIdx * (kOptionW + kOptionSpacing);
    const float y0 = (H - kOptionH) * 0.5f;
    return { x0, y0, x0 + kOptionW, y0 + kOptionH };
}

std::optional<Upgrade> LevelUpMenu::update(float dt, const core::Input& input,
                                           int W, int H) {
    ignoreTimer_ = std::max(0.0f, ignoreTimer_ - dt);

    const float mx = static_cast<float>(input.mouseX());
    const float my = static_cast<float>(input.mouseY());
    const bool  click = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
    
    if (blockUntilRelease_ && !click && ignoreTimer_ <= 0.0f) {
        blockUntilRelease_ = false;
    }

    const bool  justClicked = click && !prevMouse_;
    prevMouse_ = click;

    hoverIdx_ = -1;

    // Do not process hover or selection while locked
    if (ignoreTimer_ > 0.0f || blockUntilRelease_) {
        return std::nullopt;
    }

    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        Rect r = optionRect_(W, H, i);
        if (mx >= r.x0 && mx <= r.x1 && my >= r.y0 && my <= r.y1) {
            hoverIdx_ = i;
            if (justClicked) {
                return options_[i];
            }
        }
    }

    return std::nullopt;
}

void LevelUpMenu::draw(render::Hud& hud, render::Text& text, int W, int H) {
    // Dim background.
    hud.drawRect(W, H, glm::vec2(0, 0),
                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                 glm::vec3(0.0f), 0.75f);

    {
        const std::string title = "LEVEL UP";
        const float scale = 4.0f;
        const float w = render::Text::measure(title) * scale;
        text.draw(W, H, W * 0.5f - w * 0.5f, (H - kOptionH) * 0.5f - 80.0f,
                  title, scale, glm::vec4(1.0f, 0.95f, 0.9f, 1.0f));
    }

    const glm::vec3 panelColor { 0.05f, 0.02f, 0.02f };
    const glm::vec3 borderNormal { 0.45f, 0.10f, 0.10f };
    const glm::vec3 borderHover  { 0.95f, 0.20f, 0.15f };
    const glm::vec4 nameColor { 1.0f, 0.94f, 0.92f, 1.0f };
    const glm::vec4 descColor { 0.85f, 0.75f, 0.75f, 1.0f };

    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        Rect r = optionRect_(W, H, i);
        bool hover = (i == hoverIdx_);

        // Panel background.
        hud.drawProgress(W, H, glm::vec2(r.x0, r.y0), glm::vec2(kOptionW, kOptionH),
                         1.0f, panelColor, panelColor, hover ? borderHover : borderNormal,
                         hover ? 3.0f : 1.5f, 0.95f);

        const Upgrade& up = options_[i];
        
        // Upgrade Name.
        const std::string name = up.name;
        const float nScale = 2.5f;
        const float nW = render::Text::measure(name) * nScale;
        text.draw(W, H, r.x0 + (kOptionW - nW) * 0.5f, r.y0 + 40.0f,
                  name, nScale, nameColor);

        // Upgrade Description.
        const std::string desc = up.description;
        const float dScale = 1.8f;
        const float dW = render::Text::measure(desc) * dScale;
        text.draw(W, H, r.x0 + (kOptionW - dW) * 0.5f, r.y0 + kOptionH * 0.5f,
                  desc, dScale, descColor);

        if (hover) {
            const std::string select = "[ SELECT ]";
            const float sScale = 1.5f;
            const float sW = render::Text::measure(select) * sScale;
            text.draw(W, H, r.x0 + (kOptionW - sW) * 0.5f, r.y0 + kOptionH - 40.0f,
                      select, sScale, nameColor);
        }
    }
}

}  // namespace game

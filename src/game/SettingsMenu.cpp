#include "SettingsMenu.h"

#include "core/Input.h"
#include "render/Hud.h"
#include "render/Text.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

namespace game {

namespace {
constexpr float kPanelW       = 540.0f;
constexpr float kPanelH       = 400.0f;
constexpr float kTrackHalfW   = 250.0f;
constexpr float kTrackThick   =   8.0f;
constexpr float kTrackHitPad  =  12.0f;
constexpr float kThumbHalfW   =   6.0f;
constexpr float kThumbHalfH   =  14.0f;
constexpr float kLabelOffset  = -22.0f;
constexpr float kRangeOffset  =  14.0f;
constexpr float kToggleHalfW  = 150.0f;
constexpr float kToggleHalfH  =  16.0f;
constexpr float kButtonHalfW  = 110.0f;
constexpr float kButtonHalfH  =  20.0f;
}  // namespace

void SettingsMenu::buildItems_(std::vector<Item>& out) {
    Item s1{};
    s1.kind = Item::Kind::Slider; s1.label = "FOV";
    s1.sliderValue = &settings_.hFovDeg;
    s1.sliderMin = 70.0f; s1.sliderMax = 130.0f;
    s1.yOffsetPx = 100.0f;
    out.push_back(s1);

    Item s2{};
    s2.kind = Item::Kind::Slider; s2.label = "FLASHLIGHT";
    s2.sliderValue = &settings_.flashlightDeg;
    s2.sliderMin = 10.0f; s2.sliderMax = 45.0f;
    s2.yOffsetPx = 180.0f;
    out.push_back(s2);

    Item t{};
    t.kind = Item::Kind::Toggle; t.label = "FULLSCREEN";
    t.toggleValue = &settings_.fullscreen;
    t.yOffsetPx = 250.0f;
    out.push_back(t);

    Item b{};
    b.kind = Item::Kind::Button; b.label = "EXIT GAME";
    b.action = MenuAction::ExitGame;
    b.yOffsetPx = 320.0f;
    out.push_back(b);
}

SettingsMenu::Rect SettingsMenu::trackRect_(int W, int H, const Item& it) const {
    const float cx     = W * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    const float y0     = panelY + it.yOffsetPx;
    return { cx - kTrackHalfW, y0, cx + kTrackHalfW, y0 + kTrackThick };
}

SettingsMenu::Rect SettingsMenu::hitRect_(int W, int H, const Item& it) const {
    const float cx     = W * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    const float yC     = panelY + it.yOffsetPx;

    switch (it.kind) {
        case Item::Kind::Slider: {
            const Rect t = trackRect_(W, H, it);
            return { t.x0 - kThumbHalfW, t.y0 - kTrackHitPad,
                     t.x1 + kThumbHalfW, t.y1 + kTrackHitPad };
        }
        case Item::Kind::Toggle:
            return { cx - kToggleHalfW, yC - kToggleHalfH,
                     cx + kToggleHalfW, yC + kToggleHalfH };
        case Item::Kind::Button:
            return { cx - kButtonHalfW, yC - kButtonHalfH,
                     cx + kButtonHalfW, yC + kButtonHalfH };
    }
    return {0, 0, 0, 0};
}

MenuAction SettingsMenu::update(float /*dt*/, const core::Input& input,
                                int W, int H) {
    std::vector<Item> items;
    buildItems_(items);

    const float mx    = static_cast<float>(input.mouseX());
    const float my    = static_cast<float>(input.mouseY());
    const bool  click = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
    const bool  justClicked = click && !prevMouse_;
    prevMouse_ = click;

    MenuAction action = MenuAction::None;

    if (justClicked) {
        draggingIdx_ = -1;
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const Rect r = hitRect_(W, H, items[i]);
            const bool hit = mx >= r.x0 && mx <= r.x1 && my >= r.y0 && my <= r.y1;
            if (!hit) continue;

            switch (items[i].kind) {
                case Item::Kind::Slider:
                    draggingIdx_ = i;
                    break;
                case Item::Kind::Toggle:
                    if (items[i].toggleValue) {
                        *items[i].toggleValue = !*items[i].toggleValue;
                    }
                    break;
                case Item::Kind::Button:
                    action = items[i].action;
                    break;
            }
            break;
        }
    }
    if (!click) draggingIdx_ = -1;

    if (draggingIdx_ >= 0 && draggingIdx_ < static_cast<int>(items.size())) {
        const Item& s = items[draggingIdx_];
        const Rect t = trackRect_(W, H, s);
        const float ratio = std::clamp((mx - t.x0) / (t.x1 - t.x0), 0.0f, 1.0f);
        if (s.sliderValue) {
            *s.sliderValue = s.sliderMin + ratio * (s.sliderMax - s.sliderMin);
        }
    }

    return action;
}

void SettingsMenu::draw(render::Hud& hud, render::Text& text, int W, int H) {
    const glm::vec3 bgColor    { 0.0f,  0.0f,  0.0f  };
    const glm::vec3 panelColor { 0.05f, 0.02f, 0.02f };
    const glm::vec3 panelBorder{ 0.6f,  0.1f,  0.1f  };
    const glm::vec3 trackEmpty { 0.10f, 0.07f, 0.07f };
    const glm::vec3 trackFill  { 0.85f, 0.10f, 0.10f };
    const glm::vec3 trackEdge  { 0.45f, 0.10f, 0.10f };
    const glm::vec3 thumbColor { 1.00f, 0.90f, 0.85f };
    const glm::vec3 togglePillOff { 0.10f, 0.07f, 0.07f };
    const glm::vec3 togglePillOn  { 0.85f, 0.10f, 0.10f };
    const glm::vec3 togglePillBdr { 0.55f, 0.10f, 0.10f };
    const glm::vec3 buttonFill   { 0.10f, 0.03f, 0.03f };
    const glm::vec3 buttonBorder { 0.85f, 0.10f, 0.10f };
    const glm::vec4 titleColor { 1.0f,  0.94f, 0.92f, 1.0f };
    const glm::vec4 labelColor { 0.95f, 0.85f, 0.85f, 1.0f };
    const glm::vec4 buttonText { 1.0f,  0.30f, 0.30f, 1.0f };
    const glm::vec4 hintColor  { 0.65f, 0.55f, 0.55f, 1.0f };

    hud.drawRect(W, H, glm::vec2(0, 0),
                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                 bgColor, 0.65f);

    const float panelX = W * 0.5f - kPanelW * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                     glm::vec2(kPanelW, kPanelH),
                     1.0f, panelColor, panelColor, panelBorder, 2.0f, 0.92f);

    {
        const std::string title = "SETTINGS";
        const float scale = 3.0f;
        const float w = render::Text::measure(title) * scale;
        text.draw(W, H, W * 0.5f - w * 0.5f, panelY + 24.0f, title, scale, titleColor);
    }

    std::vector<Item> items;
    buildItems_(items);

    const float cx = W * 0.5f;

    for (const Item& it : items) {
        const float yC = panelY + it.yOffsetPx;
        switch (it.kind) {
            case Item::Kind::Slider: {
                const Rect t = trackRect_(W, H, it);
                const float trackLen = t.x1 - t.x0;
                const float fillFrac = std::clamp(
                    (*it.sliderValue - it.sliderMin)
                    / (it.sliderMax - it.sliderMin), 0.0f, 1.0f);

                char buf[64];
                std::snprintf(buf, sizeof(buf), "%s  %.0f", it.label, *it.sliderValue);
                const std::string label = buf;
                const float lScale = 2.0f;
                const float lW = render::Text::measure(label) * lScale;
                text.draw(W, H, W * 0.5f - lW * 0.5f, t.y0 + kLabelOffset,
                          label, lScale, labelColor);

                hud.drawProgress(W, H,
                                 glm::vec2(t.x0, t.y0),
                                 glm::vec2(trackLen, kTrackThick),
                                 fillFrac, trackFill, trackEmpty, trackEdge,
                                 1.0f, 0.95f);

                const float thumbX = t.x0 + fillFrac * trackLen;
                const float thumbY = (t.y0 + t.y1) * 0.5f;
                hud.drawRect(W, H,
                             glm::vec2(thumbX - kThumbHalfW, thumbY - kThumbHalfH),
                             glm::vec2(kThumbHalfW * 2.0f, kThumbHalfH * 2.0f),
                             thumbColor, 0.95f);

                char loBuf[16], hiBuf[16];
                std::snprintf(loBuf, sizeof(loBuf), "%.0f", it.sliderMin);
                std::snprintf(hiBuf, sizeof(hiBuf), "%.0f", it.sliderMax);
                const std::string lo = loBuf, hi = hiBuf;
                const float rScale = 1.5f;
                text.draw(W, H, t.x0, t.y1 + kRangeOffset, lo, rScale, hintColor);
                text.draw(W, H, t.x1 - render::Text::measure(hi) * rScale,
                          t.y1 + kRangeOffset, hi, rScale, hintColor);
                break;
            }
            case Item::Kind::Toggle: {
                // Label on the left, indicator pill on the right.
                const std::string label = it.label;
                const float lScale = 2.0f;
                text.draw(W, H, cx - kToggleHalfW + 4.0f, yC - 8.0f,
                          label, lScale, labelColor);

                const bool on = it.toggleValue && *it.toggleValue;
                const float pillW = 64.0f;
                const float pillH = 22.0f;
                const float pillX = cx + kToggleHalfW - pillW - 4.0f;
                const float pillY = yC - pillH * 0.5f;
                const glm::vec3 pillCol = on ? togglePillOn : togglePillOff;
                hud.drawProgress(W, H,
                                 glm::vec2(pillX, pillY),
                                 glm::vec2(pillW, pillH),
                                 1.0f, pillCol, pillCol, togglePillBdr, 1.0f, 0.95f);
                const std::string pillTxt = on ? "ON" : "OFF";
                const float pScale = 1.5f;
                const float pTW = render::Text::measure(pillTxt) * pScale;
                text.draw(W, H, pillX + (pillW - pTW) * 0.5f,
                          pillY + (pillH - 7.0f * pScale) * 0.5f,
                          pillTxt, pScale,
                          on ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : labelColor);
                break;
            }
            case Item::Kind::Button: {
                hud.drawProgress(W, H,
                                 glm::vec2(cx - kButtonHalfW, yC - kButtonHalfH),
                                 glm::vec2(kButtonHalfW * 2.0f, kButtonHalfH * 2.0f),
                                 1.0f, buttonFill, buttonFill, buttonBorder, 2.0f, 0.95f);
                const std::string label = it.label;
                const float bScale = 2.0f;
                const float bW = render::Text::measure(label) * bScale;
                text.draw(W, H, cx - bW * 0.5f, yC - 7.0f,
                          label, bScale, buttonText);
                break;
            }
        }
    }

    {
        const std::string hint = "ESC TO CLOSE";
        const float scale = 1.5f;
        const float w = render::Text::measure(hint) * scale;
        text.draw(W, H, W * 0.5f - w * 0.5f,
                  panelY + kPanelH - 28.0f, hint, scale, hintColor);
    }
}

}  // namespace game

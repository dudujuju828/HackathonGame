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
constexpr float kPanelH       = 320.0f;
constexpr float kTrackHalfW   = 250.0f;
constexpr float kTrackThick   =   8.0f;
constexpr float kTrackHitPad  =  12.0f;
constexpr float kThumbHalfW   =   6.0f;
constexpr float kThumbHalfH   =  14.0f;
constexpr float kLabelOffset  = -22.0f;  // label baseline y-offset from track top
constexpr float kRangeOffset  =  14.0f;  // range labels y-offset from track bottom
}  // namespace

void SettingsMenu::buildSliders_(std::vector<SliderDef>& out) {
    out.push_back({ "FOV",        &settings_.hFovDeg,        70.0f, 130.0f,  90.0f });
    out.push_back({ "FLASHLIGHT", &settings_.flashlightDeg,  10.0f,  45.0f, 180.0f });
}

SettingsMenu::TrackRect SettingsMenu::trackOf_(int W, int H, const SliderDef& s) const {
    const float cx     = W * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    const float y0     = panelY + s.yOffsetPx;
    return { cx - kTrackHalfW, y0, cx + kTrackHalfW, y0 + kTrackThick };
}

void SettingsMenu::update(float /*dt*/, const core::Input& input,
                          int W, int H) {
    std::vector<SliderDef> sliders;
    buildSliders_(sliders);

    const float mx    = static_cast<float>(input.mouseX());
    const float my    = static_cast<float>(input.mouseY());
    const bool  click = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);

    if (click && !prevMouse_) {
        draggingIdx_ = -1;
        for (int i = 0; i < static_cast<int>(sliders.size()); ++i) {
            const TrackRect t = trackOf_(W, H, sliders[i]);
            const bool hit =
                mx >= t.x0 - kThumbHalfW && mx <= t.x1 + kThumbHalfW &&
                my >= t.y0 - kTrackHitPad && my <= t.y1 + kTrackHitPad;
            if (hit) { draggingIdx_ = i; break; }
        }
    }
    if (!click) draggingIdx_ = -1;
    prevMouse_ = click;

    if (draggingIdx_ >= 0 && draggingIdx_ < static_cast<int>(sliders.size())) {
        const SliderDef& s = sliders[draggingIdx_];
        const TrackRect t  = trackOf_(W, H, s);
        const float ratio  = std::clamp((mx - t.x0) / (t.x1 - t.x0), 0.0f, 1.0f);
        *s.value = s.min + ratio * (s.max - s.min);
    }
}

void SettingsMenu::draw(render::Hud& hud, render::Text& text, int W, int H) {
    const glm::vec3 bgColor    { 0.0f,  0.0f,  0.0f  };
    const glm::vec3 panelColor { 0.05f, 0.02f, 0.02f };
    const glm::vec3 panelBorder{ 0.6f,  0.1f,  0.1f  };
    const glm::vec3 trackEmpty { 0.10f, 0.07f, 0.07f };
    const glm::vec3 trackFill  { 0.85f, 0.10f, 0.10f };
    const glm::vec3 trackEdge  { 0.45f, 0.10f, 0.10f };
    const glm::vec3 thumbColor { 1.00f, 0.90f, 0.85f };
    const glm::vec4 titleColor { 1.0f,  0.94f, 0.92f, 1.0f };
    const glm::vec4 labelColor { 0.95f, 0.85f, 0.85f, 1.0f };
    const glm::vec4 hintColor  { 0.65f, 0.55f, 0.55f, 1.0f };

    hud.drawRect(W, H, glm::vec2(0, 0),
                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                 bgColor, 0.65f);

    const float panelX = W * 0.5f - kPanelW * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                     glm::vec2(kPanelW, kPanelH),
                     /*fill=*/1.0f, panelColor, panelColor, panelBorder,
                     /*borderPx=*/2.0f, /*opacity=*/0.92f);

    // Title.
    {
        const std::string title  = "SETTINGS";
        const float       scale  = 3.0f;
        const float       w      = render::Text::measure(title) * scale;
        text.draw(W, H, W * 0.5f - w * 0.5f, panelY + 24.0f,
                  title, scale, titleColor);
    }

    std::vector<SliderDef> sliders;
    buildSliders_(sliders);

    for (const SliderDef& s : sliders) {
        const TrackRect t        = trackOf_(W, H, s);
        const float     trackLen = t.x1 - t.x0;
        const float     fillFrac = std::clamp(
            (*s.value - s.min) / (s.max - s.min), 0.0f, 1.0f);

        // "FOV  100" label centred above the track.
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s  %.0f", s.label, *s.value);
        const std::string label  = buf;
        const float       lScale = 2.0f;
        const float       lW     = render::Text::measure(label) * lScale;
        text.draw(W, H, W * 0.5f - lW * 0.5f, t.y0 + kLabelOffset,
                  label, lScale, labelColor);

        // Track.
        hud.drawProgress(W, H,
                         glm::vec2(t.x0, t.y0),
                         glm::vec2(trackLen, kTrackThick),
                         fillFrac,
                         trackFill, trackEmpty, trackEdge,
                         /*borderPx=*/1.0f, /*opacity=*/0.95f);

        // Thumb.
        const float thumbX = t.x0 + fillFrac * trackLen;
        const float thumbY = (t.y0 + t.y1) * 0.5f;
        hud.drawRect(W, H,
                     glm::vec2(thumbX - kThumbHalfW, thumbY - kThumbHalfH),
                     glm::vec2(kThumbHalfW * 2.0f, kThumbHalfH * 2.0f),
                     thumbColor, 0.95f);

        // Range labels.
        char loBuf[16], hiBuf[16];
        std::snprintf(loBuf, sizeof(loBuf), "%.0f", s.min);
        std::snprintf(hiBuf, sizeof(hiBuf), "%.0f", s.max);
        const std::string lo = loBuf;
        const std::string hi = hiBuf;
        const float       rScale = 1.5f;
        text.draw(W, H, t.x0, t.y1 + kRangeOffset, lo, rScale, hintColor);
        text.draw(W, H,
                  t.x1 - render::Text::measure(hi) * rScale,
                  t.y1 + kRangeOffset, hi, rScale, hintColor);
    }

    // Footer hint.
    {
        const std::string hint  = "ESC TO CLOSE";
        const float       scale = 1.5f;
        const float       w     = render::Text::measure(hint) * scale;
        text.draw(W, H, W * 0.5f - w * 0.5f,
                  panelY + kPanelH - 28.0f, hint, scale, hintColor);
    }
}

}  // namespace game

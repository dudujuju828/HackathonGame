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
constexpr float kTrackHalfW   = 250.0f;
constexpr float kTrackThick   = 8.0f;
constexpr float kTrackHitPad  = 12.0f;  // generous vertical hit area
constexpr float kThumbHalfW   = 6.0f;
constexpr float kThumbHalfH   = 14.0f;
}  // namespace

SettingsMenu::SliderRect SettingsMenu::fovTrack_(int W, int H) const {
    const float cx = W * 0.5f;
    const float cy = H * 0.5f;
    const float yCentre = cy + 30.0f;
    return {
        cx - kTrackHalfW,
        yCentre - kTrackThick * 0.5f,
        cx + kTrackHalfW,
        yCentre + kTrackThick * 0.5f,
    };
}

void SettingsMenu::update(float /*dt*/, const core::Input& input,
                          int W, int H) {
    const SliderRect t = fovTrack_(W, H);
    const float mx = static_cast<float>(input.mouseX());
    const float my = static_cast<float>(input.mouseY());
    const bool  click = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);

    const bool overTrack =
        (mx >= t.x0 - kThumbHalfW && mx <= t.x1 + kThumbHalfW &&
         my >= t.y0 - kTrackHitPad && my <= t.y1 + kTrackHitPad);

    if (click && !prevMouse_ && overTrack) dragging_ = true;
    if (!click)                            dragging_ = false;
    prevMouse_ = click;

    if (dragging_) {
        const float trackLen = t.x1 - t.x0;
        const float ratio    = std::clamp((mx - t.x0) / trackLen, 0.0f, 1.0f);
        settings_.hFovDeg = settings_.hFovMinDeg
            + ratio * (settings_.hFovMaxDeg - settings_.hFovMinDeg);
    }
}

void SettingsMenu::draw(render::Hud& hud, render::Text& text, int W, int H) {
    const glm::vec3 bgColor    { 0.0f, 0.0f, 0.0f };
    const glm::vec3 panelColor { 0.05f, 0.02f, 0.02f };
    const glm::vec3 panelBorder{ 0.6f, 0.1f, 0.1f };
    const glm::vec3 trackEmpty { 0.10f, 0.07f, 0.07f };
    const glm::vec3 trackFill  { 0.85f, 0.10f, 0.10f };
    const glm::vec3 trackEdge  { 0.45f, 0.10f, 0.10f };
    const glm::vec3 thumbColor { 1.00f, 0.90f, 0.85f };
    const glm::vec4 titleColor { 1.0f, 0.94f, 0.92f, 1.0f };
    const glm::vec4 labelColor { 0.95f, 0.85f, 0.85f, 1.0f };
    const glm::vec4 hintColor  { 0.65f, 0.55f, 0.55f, 1.0f };

    // Full-screen dim.
    hud.drawRect(W, H, glm::vec2(0, 0),
                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                 bgColor, 0.65f);

    // Centred panel.
    const float panelW = 540.0f;
    const float panelH = 240.0f;
    const float panelX = W * 0.5f - panelW * 0.5f;
    const float panelY = H * 0.5f - panelH * 0.5f;
    hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                     glm::vec2(panelW, panelH),
                     /*fill=*/1.0f, panelColor, panelColor, panelBorder,
                     /*borderPx=*/2.0f, /*opacity=*/0.92f);

    // Title.
    const std::string title = "SETTINGS";
    const float titleScale = 3.0f;
    const float titleW = render::Text::measure(title) * titleScale;
    text.draw(W, H, W * 0.5f - titleW * 0.5f, panelY + 24.0f,
              title, titleScale, titleColor);

    // FOV value label.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "FOV  %.0f", settings_.hFovDeg);
    const std::string label = buf;
    const float labelScale = 2.0f;
    const float labelW = render::Text::measure(label) * labelScale;
    text.draw(W, H, W * 0.5f - labelW * 0.5f,
              H * 0.5f - 12.0f, label, labelScale, labelColor);

    // Slider track + filled portion.
    const SliderRect t = fovTrack_(W, H);
    const float trackLen = t.x1 - t.x0;
    const float trackY   = t.y0;
    const float fillFrac =
        (settings_.hFovDeg - settings_.hFovMinDeg)
        / (settings_.hFovMaxDeg - settings_.hFovMinDeg);

    hud.drawProgress(W, H,
                     glm::vec2(t.x0, trackY),
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

    // Min / max range labels under the track.
    const std::string lo = "70";
    const std::string hi = "130";
    text.draw(W, H, t.x0,                              t.y1 + 14.0f,
              lo, 1.5f, hintColor);
    text.draw(W, H, t.x1 - render::Text::measure(hi) * 1.5f,
              t.y1 + 14.0f, hi, 1.5f, hintColor);

    // Footer hint.
    const std::string hint = "ESC TO CLOSE";
    const float hintScale = 1.5f;
    const float hintW = render::Text::measure(hint) * hintScale;
    text.draw(W, H, W * 0.5f - hintW * 0.5f,
              panelY + panelH - 28.0f, hint, hintScale, hintColor);
}

}  // namespace game

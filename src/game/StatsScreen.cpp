#include "StatsScreen.h"

#include "render/Hud.h"
#include "render/Text.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace game {

namespace {
constexpr float kPanelW  = 780.0f;
constexpr float kPanelH  = 480.0f;
constexpr float kSplitX  = kPanelW * 0.5f;   // 390 — left/right boundary
constexpr float kInset   =  24.0f;            // content inset from panel edges
constexpr float kBarH    =  12.0f;
}

void StatsScreen::draw(render::Hud& hud, render::Text& text,
                       int W, int H, const PlayerStats& st) const {
    // Palette — same dark-red family as the rest of the UI.
    const glm::vec3 bgColor     { 0.00f, 0.00f, 0.00f };
    const glm::vec3 panelColor  { 0.05f, 0.02f, 0.02f };
    const glm::vec3 panelBorder { 0.60f, 0.10f, 0.10f };
    const glm::vec3 divColor    { 0.30f, 0.08f, 0.08f };
    const glm::vec3 hpFill      { 0.78f, 0.10f, 0.10f };
    const glm::vec3 hpEmpty     { 0.05f, 0.02f, 0.02f };
    const glm::vec3 hpBorder    { 0.55f, 0.20f, 0.20f };
    const glm::vec3 stFill      { 0.20f, 0.75f, 0.85f };
    const glm::vec3 stEmpty     { 0.02f, 0.06f, 0.08f };
    const glm::vec3 stBorder    { 0.15f, 0.45f, 0.55f };
    const glm::vec3 slotColor   { 0.08f, 0.04f, 0.04f };
    const glm::vec3 slotBorder  { 0.35f, 0.08f, 0.08f };
    const glm::vec4 titleColor  { 1.00f, 0.94f, 0.92f, 1.00f };
    const glm::vec4 labelColor  { 0.95f, 0.85f, 0.85f, 1.00f };
    const glm::vec4 hintColor   { 0.65f, 0.55f, 0.55f, 1.00f };

    // Dark overlay behind the panel.
    hud.drawRect(W, H, glm::vec2(0.0f, 0.0f),
                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                 bgColor, 0.72f);

    // Main panel.
    const float panelX = W * 0.5f - kPanelW * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                     glm::vec2(kPanelW, kPanelH),
                     1.0f, panelColor, panelColor, panelBorder, 2.0f, 0.95f);

    // Vertical divider line.
    const float divLineX = panelX + kSplitX - 1.0f;
    hud.drawRect(W, H, glm::vec2(divLineX, panelY + 8.0f),
                 glm::vec2(2.0f, kPanelH - 16.0f), divColor, 0.90f);

    // ------------------------------------------------------------------ //
    //  LEFT PANEL — STATS
    // ------------------------------------------------------------------ //
    const float lx    = panelX + kInset;           // left content x
    const float lbarW = kSplitX - kInset * 2.0f;  // bar / content width
    float y = panelY + 28.0f;

    {
        const std::string title = "STATS";
        const float sc = 3.0f;
        const float tw = render::Text::measure(title) * sc;
        text.draw(W, H, panelX + (kSplitX - tw) * 0.5f, y, title, sc, titleColor);
    }
    y += 48.0f;

    // HEALTH
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "HEALTH   %.0f / %.0f", st.health, st.maxHealth);
        text.draw(W, H, lx, y, buf, 2.0f, labelColor);
        y += 20.0f;
        const float frac = (st.maxHealth > 0.0f)
            ? std::clamp(st.health / st.maxHealth, 0.0f, 1.0f) : 0.0f;
        hud.drawProgress(W, H, glm::vec2(lx, y), glm::vec2(lbarW, kBarH),
                         frac, hpFill, hpEmpty, hpBorder, 1.0f, 0.92f);
        y += kBarH + 22.0f;
    }

    // DMG
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "DMG      %.1f / syringe", st.damage);
        text.draw(W, H, lx, y, buf, 2.0f, labelColor);
        y += 34.0f;
    }

    // ATK SPD
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "ATK SPD  %.1f / sec", st.attackSpeed);
        text.draw(W, H, lx, y, buf, 2.0f, labelColor);
        y += 34.0f;
    }

    // STAMINA
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "STAMINA  %.0f / %.0f", st.stamina, st.maxStamina);
        text.draw(W, H, lx, y, buf, 2.0f, labelColor);
        y += 20.0f;
        const float frac = (st.maxStamina > 0.0f)
            ? std::clamp(st.stamina / st.maxStamina, 0.0f, 1.0f) : 0.0f;
        hud.drawProgress(W, H, glm::vec2(lx, y), glm::vec2(lbarW, kBarH),
                         frac, stFill, stEmpty, stBorder, 1.0f, 0.92f);
    }

    // ------------------------------------------------------------------ //
    //  RIGHT PANEL — INVENTORY
    // ------------------------------------------------------------------ //
    const float rx    = panelX + kSplitX + kInset;
    const float rw    = kSplitX - kInset;          // right content width
    float ry = panelY + 28.0f;

    {
        const std::string title = "INVENTORY";
        const float sc = 3.0f;
        const float tw = render::Text::measure(title) * sc;
        text.draw(W, H, rx + (rw - tw) * 0.5f, ry, title, sc, titleColor);
    }
    ry += 52.0f;

    // 4-column x 3-row inventory grid.
    const int   cols     = 4;
    const int   rows     = 3;
    const float slotSize = 68.0f;
    const float slotGap  =  8.0f;
    const float gridW    = cols * slotSize + (cols - 1) * slotGap;
    const float gridX    = rx + (rw - gridW) * 0.5f;

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const float sx = gridX + col * (slotSize + slotGap);
            const float sy = ry    + row * (slotSize + slotGap);
            hud.drawProgress(W, H, glm::vec2(sx, sy),
                             glm::vec2(slotSize, slotSize),
                             0.0f, slotColor, slotColor, slotBorder, 2.0f, 0.90f);
        }
    }

    // Close hint centred at panel bottom.
    {
        const std::string hint = "[B] TO CLOSE";
        const float sc = 1.5f;
        const float tw = render::Text::measure(hint) * sc;
        text.draw(W, H, W * 0.5f - tw * 0.5f,
                  panelY + kPanelH - 28.0f, hint, sc, hintColor);
    }
}

}  // namespace game

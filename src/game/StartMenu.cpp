#include "StartMenu.h"

#include "core/Input.h"
#include "render/Hud.h"
#include "render/Text.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace game {

namespace {

constexpr float kPanelW   = 900.0f;
constexpr float kPanelH   = 660.0f;
constexpr float kInset    =  36.0f;
constexpr float kContentW = kPanelW - kInset * 2.0f;

constexpr float kTitleScale = 4.0f;
constexpr float kLabelScale = 2.0f;
constexpr float kBodyScale  = 1.7f;

constexpr float kBtnW = 340.0f;
constexpr float kBtnH =  52.0f;

} // namespace

bool StartMenu::load(const std::string& path) {
    lines_.clear();
    std::ifstream f(path);
    if (!f.is_open()) {
        std::fprintf(stderr, "[StartMenu] cannot open %s\n", path.c_str());
        return false;
    }
    std::string raw;
    while (std::getline(f, raw)) {
        Line ln;
        // Trim trailing carriage return (Windows line endings).
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();

        if (raw.empty()) {
            ln.isEmpty = true;
        } else {
            ln.text    = raw;
            ln.isLabel = (raw.back() == ':');
        }
        lines_.push_back(std::move(ln));
    }
    // Drop trailing blank lines.
    while (!lines_.empty() && lines_.back().isEmpty)
        lines_.pop_back();
    return !lines_.empty();
}

std::vector<std::string> StartMenu::wrapText(const std::string& s, float maxPx, float scale) {
    std::vector<std::string> out;
    std::istringstream stream(s);
    std::string word, current;
    while (stream >> word) {
        std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && render::Text::measure(candidate) * scale > maxPx) {
            out.push_back(current);
            current = word;
        } else {
            current = std::move(candidate);
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

bool StartMenu::update(const core::Input& input, int W, int H) {
    const float panelX = W * 0.5f - kPanelW * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    const float btnX   = W * 0.5f - kBtnW * 0.5f;
    const float btnY   = panelY + kPanelH - kInset - kBtnH;

    const float mx = static_cast<float>(input.mouseX());
    const float my = static_cast<float>(input.mouseY());
    hovering_ = mx >= btnX && mx <= btnX + kBtnW && my >= btnY && my <= btnY + kBtnH;

    const bool click = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
    const bool fired = click && !prevClick_ && hovering_;
    prevClick_ = click;
    return fired;
}

void StartMenu::draw(render::Hud& hud, render::Text& text, int W, int H) const {
    const glm::vec3 bgColor     { 0.00f, 0.00f, 0.00f };
    const glm::vec3 panelColor  { 0.04f, 0.01f, 0.01f };
    const glm::vec3 panelBorder { 0.55f, 0.10f, 0.10f };
    const glm::vec3 divColor    { 0.28f, 0.06f, 0.06f };
    const glm::vec3 btnNorm     { 0.08f, 0.02f, 0.02f };
    const glm::vec3 btnHover    { 0.18f, 0.04f, 0.04f };
    const glm::vec3 btnBorderN  { 0.50f, 0.12f, 0.12f };
    const glm::vec3 btnBorderH  { 0.95f, 0.22f, 0.18f };
    const glm::vec4 titleColor  { 1.00f, 0.90f, 0.85f, 1.0f };
    const glm::vec4 labelColor  { 0.95f, 0.72f, 0.60f, 1.0f };
    const glm::vec4 bodyColor   { 0.88f, 0.82f, 0.80f, 1.0f };
    const glm::vec4 btnTxtNorm  { 0.95f, 0.85f, 0.82f, 1.0f };
    const glm::vec4 btnTxtHover { 1.00f, 0.96f, 0.94f, 1.0f };

    // Full-screen dark cover.
    hud.drawRect(W, H, glm::vec2(0.0f, 0.0f),
                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                 bgColor, 1.0f);

    const float panelX = W * 0.5f - kPanelW * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;

    hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                     glm::vec2(kPanelW, kPanelH),
                     1.0f, panelColor, panelColor, panelBorder, 2.0f, 0.97f);

    float y = panelY + kInset;

    // Title.
    {
        const std::string title = "CLASSIFIED BRIEFING";
        const float tw = render::Text::measure(title) * kTitleScale;
        text.draw(W, H, panelX + (kPanelW - tw) * 0.5f, y, title, kTitleScale, titleColor);
        y += 12.0f * kTitleScale + 16.0f;
    }

    // Divider below title.
    hud.drawRect(W, H, glm::vec2(panelX + kInset, y),
                 glm::vec2(kPanelW - kInset * 2.0f, 2.0f), divColor, 0.90f);
    y += 14.0f;

    const float lineStep  = 12.0f * kBodyScale + 4.0f;
    const float labelStep = 12.0f * kLabelScale + 4.0f;
    const float cx = panelX + kInset;

    // Briefing content.
    for (const auto& ln : lines_) {
        if (ln.isEmpty) {
            y += 14.0f;
            continue;
        }
        if (ln.isLabel) {
            text.draw(W, H, cx, y, ln.text, kLabelScale, labelColor);
            y += labelStep + 2.0f;
        } else {
            for (const auto& wrapped : wrapText(ln.text, kContentW, kBodyScale)) {
                text.draw(W, H, cx, y, wrapped, kBodyScale, bodyColor);
                y += lineStep;
            }
        }
    }

    // Divider above button.
    const float btnY = panelY + kPanelH - kInset - kBtnH;
    hud.drawRect(W, H, glm::vec2(panelX + kInset, btnY - 14.0f),
                 glm::vec2(kPanelW - kInset * 2.0f, 2.0f), divColor, 0.90f);

    // Start Mission button.
    const float btnX = W * 0.5f - kBtnW * 0.5f;
    hud.drawProgress(W, H, glm::vec2(btnX, btnY), glm::vec2(kBtnW, kBtnH),
                     1.0f,
                     hovering_ ? btnHover : btnNorm,
                     hovering_ ? btnHover : btnNorm,
                     hovering_ ? btnBorderH : btnBorderN,
                     hovering_ ? 2.5f : 1.5f,
                     0.95f);
    {
        const std::string label = "[ START MISSION ]";
        const float sc = 2.2f;
        const float lw = render::Text::measure(label) * sc;
        text.draw(W, H, W * 0.5f - lw * 0.5f,
                  btnY + (kBtnH - 12.0f * sc) * 0.5f,
                  label, sc, hovering_ ? btnTxtHover : btnTxtNorm);
    }
}

} // namespace game

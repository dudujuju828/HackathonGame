#include "Journal.h"

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

// Parse one CSV field, handling RFC-4180 double-quoted fields with embedded commas.
std::string parseField(const std::string& line, size_t& pos) {
    if (pos >= line.size()) return {};
    std::string result;
    if (line[pos] == '"') {
        ++pos;
        while (pos < line.size()) {
            if (line[pos] == '"') {
                ++pos;
                if (pos < line.size() && line[pos] == '"') {
                    result += '"';
                    ++pos;
                } else {
                    break;
                }
            } else {
                result += line[pos++];
            }
        }
        if (pos < line.size() && line[pos] == ',') ++pos;
    } else {
        size_t end = line.find(',', pos);
        if (end == std::string::npos) {
            result = line.substr(pos);
            pos    = line.size();
        } else {
            result = line.substr(pos, end - pos);
            pos    = end + 1;
        }
    }
    return result;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    size_t pos = 0;
    while (pos <= line.size()) {
        fields.push_back(parseField(line, pos));
        if (pos >= line.size()) break;
    }
    return fields;
}

constexpr float kPanelW   = 800.0f;
constexpr float kPanelH   = 540.0f;
constexpr float kInset    =  28.0f;
constexpr float kContentW = kPanelW - kInset * 2.0f;

constexpr float kTitleScale = 3.0f;
constexpr float kLabelScale = 2.0f;
constexpr float kBodyScale  = 1.8f;
constexpr float kHintScale  = 1.5f;

constexpr float kArrowBtnW = 56.0f;
constexpr float kArrowBtnH = 32.0f;

} // namespace

// ---------------------------------------------------------------------------

bool JournalScreen::load(const std::string& csvPath) {
    entries_.clear();
    std::ifstream f(csvPath);
    if (!f.is_open()) {
        std::fprintf(stderr, "[Journal] cannot open %s\n", csvPath.c_str());
        return false;
    }
    std::string line;
    std::getline(f, line); // skip header row
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto fields = splitCsvLine(line);
        if (static_cast<int>(fields.size()) < 5) continue;
        JournalEntry e;
        e.species     = fields[0];
        e.disease     = fields[1];
        e.description = fields[2];
        e.symptoms    = fields[3];
        e.treatment   = fields[4];
        entries_.push_back(std::move(e));
    }
    return !entries_.empty();
}

void JournalScreen::open() {
    page_      = std::max(0, std::min(page_, unlockedCount_ - 1));
    prevLeft_  = false;
    prevRight_ = false;
    prevClick_ = false;
}

void JournalScreen::unlock() {
    if (unlockedCount_ >= static_cast<int>(entries_.size())) return;
    toastDisease_ = entries_[unlockedCount_].disease;
    ++unlockedCount_;
    toastTimer_ = 4.0f;
}

void JournalScreen::tickToast(float dt) {
    if (toastTimer_ > 0.0f) toastTimer_ -= dt;
}

void JournalScreen::resetProgress() {
    unlockedCount_ = 0;
    toastTimer_    = 0.0f;
    page_          = 0;
}

std::vector<std::string> JournalScreen::wrapText(const std::string& s,
                                                  float maxPx, float scale) {
    std::vector<std::string> lines;
    std::istringstream stream(s);
    std::string word, current;
    while (stream >> word) {
        std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && render::Text::measure(candidate) * scale > maxPx) {
            lines.push_back(current);
            current = word;
        } else {
            current = std::move(candidate);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

void JournalScreen::update(const core::Input& input, int W, int H) {
    if (entries_.empty()) return;

    const bool curLeft  = input.key(GLFW_KEY_LEFT);
    const bool curRight = input.key(GLFW_KEY_RIGHT);
    if (curLeft  && !prevLeft_  && page_ > 0)
        --page_;
    if (curRight && !prevRight_ && page_ < unlockedCount_ - 1)
        ++page_;
    prevLeft_  = curLeft;
    prevRight_ = curRight;

    // Mouse click on arrow buttons — positions mirror draw().
    const float panelX = W * 0.5f - kPanelW * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    const float navY   = panelY + kPanelH - 54.0f;
    const float lBtnX  = panelX + kInset;
    const float rBtnX  = panelX + kPanelW - kInset - kArrowBtnW;

    const bool curClick = input.mouseButton(GLFW_MOUSE_BUTTON_LEFT);
    if (curClick && !prevClick_) {
        const float mx = static_cast<float>(input.mouseX());
        const float my = static_cast<float>(input.mouseY());
        auto hit = [&](float bx, float by, float bw, float bh) {
            return mx >= bx && mx <= bx + bw && my >= by && my <= by + bh;
        };
        if (hit(lBtnX, navY, kArrowBtnW, kArrowBtnH) && page_ > 0)
            --page_;
        if (hit(rBtnX, navY, kArrowBtnW, kArrowBtnH) && page_ < unlockedCount_ - 1)
            ++page_;
    }
    prevClick_ = curClick;
}

void JournalScreen::draw(render::Hud& hud, render::Text& text, int W, int H) const {
    if (entries_.empty()) return;

    const glm::vec3 bgColor    { 0.00f, 0.00f, 0.00f };
    const glm::vec3 panelColor { 0.03f, 0.02f, 0.07f };
    const glm::vec3 panelBorder{ 0.38f, 0.22f, 0.65f };
    const glm::vec3 divColor   { 0.18f, 0.10f, 0.32f };
    const glm::vec3 btnColor   { 0.10f, 0.06f, 0.20f };
    const glm::vec3 btnBorder  { 0.50f, 0.28f, 0.75f };
    const glm::vec3 btnDim     { 0.05f, 0.04f, 0.08f };
    const glm::vec3 btnDimBord { 0.18f, 0.12f, 0.25f };
    const glm::vec4 titleColor { 1.00f, 0.95f, 1.00f, 1.0f };
    const glm::vec4 labelColor { 0.78f, 0.65f, 1.00f, 1.0f };
    const glm::vec4 bodyColor  { 0.90f, 0.88f, 0.96f, 1.0f };
    const glm::vec4 hintColor  { 0.52f, 0.44f, 0.68f, 1.0f };
    const glm::vec4 btnTxt     { 1.00f, 1.00f, 1.00f, 1.0f };

    if (unlockedCount_ == 0) {
        hud.drawRect(W, H, glm::vec2(0.0f, 0.0f),
                     glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                     bgColor, 0.75f);
        const float panelX = W * 0.5f - kPanelW * 0.5f;
        const float panelY = H * 0.5f - kPanelH * 0.5f;
        hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                         glm::vec2(kPanelW, kPanelH),
                         1.0f, panelColor, panelColor, panelBorder, 2.0f, 0.95f);
        const std::string msg = "NO ENTRIES UNLOCKED YET";
        const float mw = render::Text::measure(msg) * kLabelScale;
        text.draw(W, H, W * 0.5f - mw * 0.5f, H * 0.5f - 12.0f * kLabelScale * 0.5f,
                  msg, kLabelScale, hintColor);
        const std::string hint = "[J] TO CLOSE";
        const float hw = render::Text::measure(hint) * kHintScale;
        text.draw(W, H, W * 0.5f - hw * 0.5f,
                  panelY + kPanelH - 20.0f, hint, kHintScale, hintColor);
        return;
    }

    // Dark overlay.
    hud.drawRect(W, H, glm::vec2(0.0f, 0.0f),
                 glm::vec2(static_cast<float>(W), static_cast<float>(H)),
                 bgColor, 0.75f);

    const float panelX = W * 0.5f - kPanelW * 0.5f;
    const float panelY = H * 0.5f - kPanelH * 0.5f;
    hud.drawProgress(W, H, glm::vec2(panelX, panelY),
                     glm::vec2(kPanelW, kPanelH),
                     1.0f, panelColor, panelColor, panelBorder, 2.0f, 0.95f);

    const JournalEntry& e = entries_[page_];
    const float cx = panelX + kInset;
    float y = panelY + 22.0f;

    // Title: Disease name only
    {
        const float tw = render::Text::measure(e.disease) * kTitleScale;
        text.draw(W, H, panelX + (kPanelW - tw) * 0.5f, y, e.disease, kTitleScale, titleColor);
        y += 12.0f * kTitleScale + 14.0f;
    }

    // Top divider.
    hud.drawRect(W, H, glm::vec2(cx, y),
                 glm::vec2(kPanelW - kInset * 2.0f, 2.0f), divColor, 0.90f);
    y += 14.0f;

    const float lineStep = 12.0f * kBodyScale + 4.0f;

    auto drawSection = [&](const char* label, const std::string& content) {
        text.draw(W, H, cx, y, label, kLabelScale, labelColor);
        y += 12.0f * kLabelScale + 6.0f;

        for (const auto& ln : wrapText(content, kContentW - 10.0f, kBodyScale)) {
            text.draw(W, H, cx + 10.0f, y, ln, kBodyScale, bodyColor);
            y += lineStep;
        }
        y += 4.0f;

        hud.drawRect(W, H, glm::vec2(cx, y),
                     glm::vec2(kPanelW - kInset * 2.0f, 1.0f), divColor, 0.65f);
        y += 12.0f;
    };

    drawSection("SYMPTOMS",         e.symptoms);
    drawSection("DESCRIPTION",      e.description);
    drawSection("TREATMENT",        e.treatment);
    drawSection("SPECIES AFFECTED", e.species);

    // Navigation row.
    const float navY  = panelY + kPanelH - 54.0f;
    const float lBtnX = panelX + kInset;
    const float rBtnX = panelX + kPanelW - kInset - kArrowBtnW;

    auto drawArrow = [&](float bx, float active, const char* label) {
        const glm::vec3 fill   = active ? btnColor : btnDim;
        const glm::vec3 border = active ? btnBorder : btnDimBord;
        hud.drawProgress(W, H, glm::vec2(bx, navY), glm::vec2(kArrowBtnW, kArrowBtnH),
                         1.0f, fill, fill, border, 1.5f, 0.92f);
        const float lw = render::Text::measure(label) * 2.5f;
        text.draw(W, H, bx + (kArrowBtnW - lw) * 0.5f,
                  navY + (kArrowBtnH - 12.0f * 2.5f) * 0.5f,
                  label, 2.5f, active ? btnTxt : hintColor);
    };

    drawArrow(lBtnX, page_ > 0 ? 1.0f : 0.0f, "<");
    drawArrow(rBtnX, page_ < (int)entries_.size() - 1 ? 1.0f : 0.0f, ">");

    // Page indicator centred between arrows.
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "PAGE %d / %d", page_ + 1, unlockedCount_);
        const float pw = render::Text::measure(buf) * kHintScale;
        text.draw(W, H, W * 0.5f - pw * 0.5f,
                  navY + (kArrowBtnH - 12.0f * kHintScale) * 0.5f,
                  buf, kHintScale, hintColor);
    }

    // Close hint.
    {
        const std::string hint = "[J] TO CLOSE";
        const float hw = render::Text::measure(hint) * kHintScale;
        text.draw(W, H, W * 0.5f - hw * 0.5f,
                  panelY + kPanelH - 20.0f, hint, kHintScale, hintColor);
    }
}

void JournalScreen::drawToast(render::Hud& hud, render::Text& text, int W, int H) const {
    if (toastTimer_ <= 0.0f || toastDisease_.empty()) return;

    constexpr float kToastW      = 340.0f;
    constexpr float kToastH      = 72.0f;
    constexpr float kToastMargin = 20.0f;
    constexpr float kFadeSec     = 1.0f;

    const float alpha = toastTimer_ < kFadeSec
                        ? toastTimer_ / kFadeSec
                        : 1.0f;

    const float tx = static_cast<float>(W) - kToastW - kToastMargin;
    const float ty = static_cast<float>(H) * 0.25f;

    const glm::vec3 fill  { 0.04f, 0.02f, 0.10f };
    const glm::vec3 border{ 0.50f, 0.28f, 0.85f };
    hud.drawProgress(W, H, glm::vec2(tx, ty), glm::vec2(kToastW, kToastH),
                     1.0f, fill, fill, border, 1.5f, alpha * 0.93f);

    const glm::vec4 accentCol{ 0.78f, 0.65f, 1.00f, alpha };
    const glm::vec4 nameCol  { 1.00f, 0.95f, 1.00f, alpha };
    const glm::vec4 hintCol  { 0.52f, 0.44f, 0.68f, alpha };

    constexpr float kAccentScale = 1.5f;
    constexpr float kNameScale   = 2.0f;
    constexpr float kHintS       = 1.4f;

    const std::string header = "NEW JOURNAL ENTRY";
    const float hw = render::Text::measure(header) * kAccentScale;
    text.draw(W, H, tx + (kToastW - hw) * 0.5f, ty + 8.0f,
              header, kAccentScale, accentCol);

    const float nw = render::Text::measure(toastDisease_) * kNameScale;
    text.draw(W, H, tx + (kToastW - nw) * 0.5f, ty + 8.0f + 12.0f * kAccentScale + 4.0f,
              toastDisease_, kNameScale, nameCol);

    const std::string hint = "Press J to view";
    const float hiw = render::Text::measure(hint) * kHintS;
    text.draw(W, H, tx + (kToastW - hiw) * 0.5f, ty + kToastH - 12.0f * kHintS - 6.0f,
              hint, kHintS, hintCol);
}

} // namespace game

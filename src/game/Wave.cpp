#include "Wave.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace game {

namespace {

// Reads the next whitespace-delimited token, with support for "double
// quoted" strings (which become a single token, quotes stripped).
bool nextToken(std::istringstream& iss, std::string& out) {
    out.clear();
    char c;
    while (iss.get(c)) {
        if (c == '"') {
            std::getline(iss, out, '"');
            return true;
        }
        if (!std::isspace(static_cast<unsigned char>(c))) {
            out.push_back(c);
            break;
        }
    }
    if (out.empty() && !iss.good()) return false;
    while (iss.get(c)) {
        if (std::isspace(static_cast<unsigned char>(c))) break;
        out.push_back(c);
    }
    return !out.empty();
}

}  // namespace

bool loadWaves(const std::string& path, std::vector<WaveDef>& out) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "[wave] cannot open '%s'\n", path.c_str());
        return false;
    }

    out.clear();
    std::string line;
    int lineNo = 0;
    WaveDef* current = nullptr;

    while (std::getline(f, line)) {
        ++lineNo;
        // Strip comments and trim leading whitespace.
        auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        size_t i = 0;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i >= line.size()) continue;
        line = line.substr(i);

        std::istringstream iss(line);
        std::string head;
        if (!nextToken(iss, head)) continue;

        if (head == "WAVE") {
            std::string name, duration;
            if (!nextToken(iss, name) || !nextToken(iss, duration)) {
                std::fprintf(stderr, "[wave] %s:%d malformed WAVE line\n", path.c_str(), lineNo);
                continue;
            }
            WaveDef w;
            w.name     = name;
            w.duration = std::strtof(duration.c_str(), nullptr);
            out.push_back(std::move(w));
            current = &out.back();
        } else if (head == "GROUP") {
            if (!current) {
                std::fprintf(stderr, "[wave] %s:%d GROUP outside WAVE\n", path.c_str(), lineNo);
                continue;
            }
            std::string atTok;
            if (!nextToken(iss, atTok)) {
                std::fprintf(stderr, "[wave] %s:%d GROUP missing 'at' time\n", path.c_str(), lineNo);
                continue;
            }
            WaveGroup g;
            g.at = std::strtof(atTok.c_str(), nullptr);
            std::string name, countTok;
            while (nextToken(iss, name) && nextToken(iss, countTok)) {
                WaveGroup::Entry e;
                e.name  = name;
                e.count = std::atoi(countTok.c_str());
                if (e.count > 0) g.entries.push_back(std::move(e));
            }
            current->groups.push_back(std::move(g));
        } else {
            std::fprintf(stderr, "[wave] %s:%d unknown directive '%s'\n",
                         path.c_str(), lineNo, head.c_str());
        }
    }

    std::printf("[wave] loaded %zu wave(s) from '%s'\n", out.size(), path.c_str());
    return !out.empty();
}

}  // namespace game

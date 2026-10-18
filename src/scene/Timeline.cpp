#include "scene/Timeline.h"

#include "core/Log.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

namespace demo {

namespace {

constexpr double kDefaultTransitionLength = 1.0;

bool parseNumber(const std::string& text, double& out) {
    const char* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, out);
    return ec == std::errc() && ptr == end;
}

// Seconds ("7.5") or beats ("16b") converted to seconds at `bpm`.
bool parseTime(const std::string& text, double bpm, double& seconds) {
    if (!text.empty() && text.back() == 'b') {
        double beats = 0.0;
        if (!parseNumber(text.substr(0, text.size() - 1), beats)) return false;
        seconds = beats * 60.0 / bpm;
        return true;
    }
    return parseNumber(text, seconds);
}

}  // namespace

bool Timeline::load(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        log::error("Cannot open timeline {}", file.string());
        return false;
    }

    std::vector<TimelineEntry> entries;
    std::string line;
    int lineNumber = 0;
    int errors = 0;
    double start = 0.0;
    double bpm = 125.0;
    std::string music = kPlaceholderMusic;
    const std::string name = file.filename().string();

    while (std::getline(in, line)) {
        ++lineNumber;
        if (const size_t comment = line.find('#'); comment != std::string::npos) line.resize(comment);

        std::istringstream tokens(line);

        if (const size_t first = line.find_first_not_of(" \t"); first != std::string::npos && line[first] == '@') {
            std::string directive, value, extra;
            tokens >> directive >> value >> extra;
            double number = 0.0;
            if (directive == "@bpm" && extra.empty() && parseNumber(value, number) && number > 0.0) {
                bpm = number;
            } else if (directive == "@music" && !value.empty() && extra.empty()) {
                music = value;
            } else {
                log::error("{}:{}: bad directive (expected '@bpm <number>' or '@music <file|placeholder|none>')", name,
                           lineNumber);
                ++errors;
            }
            continue;
        }

        std::vector<std::string> positional;
        TimelineEntry entry;
        entry.line = lineNumber;
        bool lineOk = true;
        int tokenCount = 0;

        for (std::string token; tokens >> token; ++tokenCount) {
            if (const size_t eq = token.find('='); eq != std::string::npos) {
                double value = 0.0;
                if (eq == 0 || !parseNumber(token.substr(eq + 1), value)) {
                    log::error("{}:{}: bad parameter '{}' (expected key=number)", name, lineNumber, token);
                    lineOk = false;
                    continue;
                }
                entry.params.set(token.substr(0, eq), static_cast<float>(value));
            } else {
                positional.push_back(token);
            }
        }
        if (tokenCount == 0) continue;  // blank or comment line

        if (positional.size() < 2 || positional.size() > 4) {
            log::error("{}:{}: expected 'scene duration [transition [length]] [key=value ...]'", name, lineNumber);
            ++errors;
            continue;
        }

        entry.scene = positional[0];
        if (!parseTime(positional[1], bpm, entry.duration) || entry.duration <= 0.0) {
            log::error("{}:{}: duration must be positive seconds or beats (16b), got '{}'", name, lineNumber,
                       positional[1]);
            lineOk = false;
        }
        if (positional.size() >= 3) {
            entry.transition = positional[2];
            entry.transitionLength = kDefaultTransitionLength;
        }
        if (positional.size() == 4 &&
            (!parseTime(positional[3], bpm, entry.transitionLength) || entry.transitionLength < 0.0)) {
            log::error("{}:{}: transition length must be seconds or beats >= 0, got '{}'", name, lineNumber,
                       positional[3]);
            lineOk = false;
        }
        if (!lineOk) {
            ++errors;
            continue;
        }

        if (entry.transition == "none") entry.transitionLength = 0.0;
        entry.transitionLength = std::min(entry.transitionLength, entry.duration);
        entry.start = start;
        start += entry.duration;
        entries.push_back(std::move(entry));
    }

    if (errors > 0) {
        log::error("Timeline {} has {} error(s); keeping the previous version", name, errors);
        return false;
    }
    if (entries.empty()) {
        log::error("Timeline {} has no entries", name);
        return false;
    }

    m_entries = std::move(entries);
    m_duration = start;
    m_bpm = bpm;
    m_music = std::move(music);
    log::info("Timeline {}: {} entries, {:.1f} s, {} bpm, music: {}", name, m_entries.size(), m_duration, m_bpm, m_music);
    return true;
}

Playhead Timeline::at(double demoTime) const {
    Playhead playhead;
    if (m_entries.empty() || m_duration <= 0.0) return playhead;

    demoTime = std::max(0.0, demoTime);
    const double lap = std::floor(demoTime / m_duration);
    playhead.lapStart = lap * m_duration;
    playhead.timelineTime = std::clamp(demoTime - playhead.lapStart, 0.0, m_duration);

    const auto it = std::upper_bound(m_entries.begin(), m_entries.end(), playhead.timelineTime,
                                     [](double t, const TimelineEntry& e) { return t < e.start; });
    const int index = std::max(0, static_cast<int>(it - m_entries.begin()) - 1);
    const TimelineEntry& entry = m_entries[index];

    playhead.current = index;
    playhead.localTime = playhead.timelineTime - entry.start;

    if (entry.transitionLength > 0.0 && playhead.localTime < entry.transitionLength) {
        playhead.blend = static_cast<float>(playhead.localTime / entry.transitionLength);
        if (index > 0) {
            playhead.previous = index - 1;
        } else if (lap >= 1.0 && m_entries.size() > 1) {
            playhead.previous = static_cast<int>(m_entries.size()) - 1;  // looping: blend from the last entry
        }  // else: very first entry of the first lap fades in from black
        if (playhead.previous >= 0)
            playhead.previousLocalTime = m_entries[playhead.previous].duration + playhead.localTime;
    }
    return playhead;
}

}  // namespace demo

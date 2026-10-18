#pragma once

#include "scene/Scene.h"

#include <filesystem>
#include <string>
#include <vector>

namespace demo {

struct TimelineEntry {
    std::string scene;
    double start = 0.0;
    double duration = 0.0;
    std::string transition = "none";  // shader name in shaders/transitions/, "none" = hard cut
    double transitionLength = 0.0;     // seconds at the start of this entry spent blending in from the previous one
    SceneParams params;
    int line = 0;  // source line, for messages
};

// Where the demo is at a given time: the active entry and, during a transition, the one blending out.
struct Playhead {
    int current = -1;             // entry index; -1 when the timeline is empty
    int previous = -1;            // entry blending out; -1 = black (fade in at the very start)
    double localTime = 0.0;       // seconds into `current`
    double previousLocalTime = 0.0;
    float blend = 1.0f;           // transition progress 0..1; 1 when no transition is running
    double lapStart = 0.0;        // demo time at which the current loop of the timeline began
    double timelineTime = 0.0;    // demo time within the current loop

    bool transitioning() const { return blend < 1.0f; }
};

// Sequence of scenes parsed from a text file. One entry per line:
//   scene  duration  [transition [length]]  [key=value ...]     # comment
// Durations and transition lengths are seconds, or beats with a "b" suffix (16b).
// Directives (before the entries that depend on them):
//   @bpm 125                      tempo for beat durations and the beat grid
//   @music placeholder|none|path  path relative to the timeline's folder; default: generated placeholder song
class Timeline {
public:
    static constexpr const char* kPlaceholderMusic = "placeholder";

    // Parses `file`. On error logs every problem and keeps the previous version.
    bool load(const std::filesystem::path& file);

    const std::vector<TimelineEntry>& entries() const { return m_entries; }
    double duration() const { return m_duration; }
    double bpm() const { return m_bpm; }
    const std::string& music() const { return m_music; }

    // The timeline loops forever: demo time is wrapped by duration().
    Playhead at(double demoTime) const;

private:
    std::vector<TimelineEntry> m_entries;
    double m_duration = 0.0;
    double m_bpm = 125.0;
    std::string m_music = kPlaceholderMusic;
};

}  // namespace demo

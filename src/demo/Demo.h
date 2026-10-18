#pragma once

#include "audio/MusicPlayer.h"
#include "audio/SyncState.h"
#include "audio/SyncTrack.h"
#include "core/RenderTarget.h"
#include "core/Resources.h"
#include "post/PostChain.h"
#include "scene/Scene.h"
#include "scene/Timeline.h"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace demo {

class DemoClock;
class ShaderLib;
class ShaderProgram;

// Plays the timeline: renders the active scene (two during a transition) into HDR targets,
// blends them with the entry's transition shader and runs the post chain to the screen.
class Demo {
public:
    Demo(ShaderLib& shaders, std::filesystem::path timelineFile);
    ~Demo();
    Demo(const Demo&) = delete;
    Demo& operator=(const Demo&) = delete;

    // Once per frame before render(): timeline hot-reload (dev), entry looping, playhead.
    void update(DemoClock& clock);
    void render(double demoTime, glm::ivec2 outputSize);

    // Dev controls
    bool reloadTimeline();
    void seekToEntry(DemoClock& clock, int index);
    std::optional<int>& loopEntry() { return m_loopEntry; }  // keep replaying this entry
    PostSettings& postSettings() { return m_postSettings; }
    float& renderScale() { return m_renderScale; }
    MusicPlayer& music() { return m_music; }
    float& audioLatency() { return m_audioLatency; }  // seconds; shifts sync to match what is heard

    const Timeline& timeline() const { return m_timeline; }
    const Playhead& playhead() const { return m_playhead; }
    const SyncState& sync() const { return m_sync; }
    const std::map<std::string, std::unique_ptr<Scene>>& scenes() const { return m_scenes; }

private:
    void createScenes();
    void loadMusic();
    void updateSync();
    void renderEntry(int index, double localTime, double demoTime, RenderTarget& target);
    ShaderProgram* transition(const std::string& name);

    Resources m_resources;
    PostChain m_post;
    Timeline m_timeline;
    std::filesystem::path m_timelineFile;
    std::filesystem::file_time_type m_timelineWriteTime{};
    double m_lastTimelinePoll = -1.0;

    std::map<std::string, std::unique_ptr<Scene>> m_scenes;  // nullptr for names not in the registry
    std::map<std::string, ShaderProgram*> m_transitions;
    RenderTarget m_current{GL_RGBA16F};
    RenderTarget m_previous{GL_RGBA16F};
    RenderTarget m_blended{GL_RGBA16F};

    MusicPlayer m_music;
    SyncTrack m_syncTrack;
    SyncState m_sync;
    std::optional<std::string> m_loadedMusic;  // timeline @music value currently loaded
    float m_audioLatency = 0.0f;

    Playhead m_playhead;
    std::optional<int> m_loopEntry;
    PostSettings m_postSettings;
    float m_renderScale = 1.0f;
};

}  // namespace demo

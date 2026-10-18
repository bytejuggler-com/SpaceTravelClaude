#include "demo/Demo.h"

#include "audio/PlaceholderSong.h"
#include "core/DemoClock.h"
#include "core/Log.h"
#include "core/Shader.h"
#include "scene/SceneRegistry.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace demo {

namespace {

constexpr double kTimelinePollSeconds = 0.5;

fs::file_time_type writeTimeOf(const fs::path& path) {
    std::error_code ec;
    const auto time = fs::last_write_time(path, ec);
    return ec ? fs::file_time_type::min() : time;
}

}  // namespace

Demo::Demo(ShaderLib& shaders, fs::path timelineFile)
    : m_resources(shaders), m_post(m_resources), m_timelineFile(std::move(timelineFile)) {
    std::string names;
    for (const std::string& name : SceneRegistry::instance().names()) names += (names.empty() ? "" : ", ") + name;
    log::info("Registered scenes: {}", names);

    if (m_music.open()) m_audioLatency = static_cast<float>(m_music.deviceLatency());
    reloadTimeline();
}

Demo::~Demo() = default;

bool Demo::reloadTimeline() {
    m_timelineWriteTime = writeTimeOf(m_timelineFile);
    if (!m_timeline.load(m_timelineFile)) return false;
    createScenes();
    loadMusic();
    if (m_loopEntry && *m_loopEntry >= static_cast<int>(m_timeline.entries().size())) m_loopEntry.reset();
    return true;
}

void Demo::loadMusic() {
    const std::string& spec = m_timeline.music();
    if (m_loadedMusic == spec) return;
    m_loadedMusic = spec;
    m_music.unload();
    m_syncTrack.clear();
    if (spec == "none") return;

    std::vector<std::uint8_t> data;
    if (spec == Timeline::kPlaceholderMusic) {
        data = buildPlaceholderSong();
    } else {
        const fs::path path = m_timelineFile.parent_path() / spec;
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            log::error("Cannot open music file {}", path.string());
            return;
        }
        data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    try {
        m_music.load(data, spec);
        m_syncTrack.build(data);
    } catch (const std::exception& e) {
        log::error("Cannot load music '{}': {}", spec, e.what());
        m_music.unload();
        m_syncTrack.clear();
    }
}

void Demo::updateSync() {
    // Song position for the current point of the timeline; the song loops if it is shorter.
    double songTime = m_playhead.timelineTime;
    if (m_music.loaded()) songTime = std::fmod(songTime, m_music.duration());

    // Visuals are compared against what is heard, which lags what was rendered by the device latency.
    const double heard = songTime - m_audioLatency;
    const double bpm = m_timeline.bpm();

    m_sync = SyncState{};
    m_sync.hasMusic = m_music.loaded();
    m_sync.time = heard;
    m_sync.bpm = static_cast<float>(bpm);
    m_sync.beat = std::max(0.0, heard) * bpm / 60.0;
    m_sync.sinceBeat = static_cast<float>((m_sync.beat - std::floor(m_sync.beat)) * 60.0 / bpm);
    m_syncTrack.evaluate(heard, m_sync);
}

void Demo::createScenes() {
    for (const TimelineEntry& entry : m_timeline.entries()) {
        if (m_scenes.contains(entry.scene)) continue;
        std::unique_ptr<Scene> scene = SceneRegistry::instance().create(entry.scene);
        if (scene) {
            scene->init(m_resources);
            log::info("Scene '{}' initialized", entry.scene);
        } else {
            log::error("Timeline line {}: unknown scene '{}' (renders black)", entry.line, entry.scene);
        }
        m_scenes.emplace(entry.scene, std::move(scene));
    }
}

void Demo::update(DemoClock& clock) {
#ifdef DEMO_DEV_MODE
    const double now = static_cast<double>(SDL_GetTicks()) / 1000.0;
    if (now - m_lastTimelinePoll >= kTimelinePollSeconds) {
        m_lastTimelinePoll = now;
        if (writeTimeOf(m_timelineFile) != m_timelineWriteTime) reloadTimeline();
    }
#endif

    m_playhead = m_timeline.at(clock.time());

    if (m_loopEntry && m_playhead.current != *m_loopEntry) {
        // Ran past the entry (or wrapped into the next lap): jump back to its start within the current lap.
        clock.seek(m_playhead.lapStart + m_timeline.entries()[*m_loopEntry].start);
        m_playhead = m_timeline.at(clock.time());
    }

    // Audio only plays at normal speed; it re-seeks itself when the clock jumps.
    const bool playing = !clock.paused() && clock.speed() == 1.0;
    m_music.follow(m_playhead.timelineTime, playing);
    updateSync();
}

void Demo::seekToEntry(DemoClock& clock, int index) {
    const auto& entries = m_timeline.entries();
    if (entries.empty()) return;
    index = (index % static_cast<int>(entries.size()) + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
    clock.seek(m_playhead.lapStart + entries[index].start);
    if (m_loopEntry) m_loopEntry = index;
    m_playhead = m_timeline.at(clock.time());
}

void Demo::render(double demoTime, glm::ivec2 outputSize) {
    if (m_playhead.current < 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, outputSize.x, outputSize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    const glm::ivec2 internalSize = glm::max(glm::ivec2(glm::vec2(outputSize) * m_renderScale), glm::ivec2(1));
    m_current.resize(internalSize);

    const auto& entries = m_timeline.entries();
    const TimelineEntry& entry = entries[m_playhead.current];
    renderEntry(m_playhead.current, m_playhead.localTime, demoTime, m_current);

    const RenderTarget* output = &m_current;
    PostSettings post = m_postSettings.withOverrides(entry.params);

    if (m_playhead.transitioning()) {
        m_previous.resize(internalSize);
        m_blended.resize(internalSize);
        PostSettings previousPost = m_postSettings;
        if (m_playhead.previous >= 0) {
            renderEntry(m_playhead.previous, m_playhead.previousLocalTime, demoTime, m_previous);
            previousPost = m_postSettings.withOverrides(entries[m_playhead.previous].params);
        } else {
            m_previous.clear();  // fade in from black
        }
        post = PostSettings::lerp(previousPost, post, m_playhead.blend);

        ShaderProgram* program = transition(entry.transition);
        if (program && program->valid()) {
            m_blended.bind();
            m_previous.bindTexture(0);
            m_current.bindTexture(1);
            program->use();
            program->set("uFrom", 0);
            program->set("uTo", 1);
            program->set("uProgress", m_playhead.blend);
            program->set("uTime", static_cast<float>(demoTime));
            program->set("uResolution", glm::vec2(internalSize));
            m_resources.drawFullscreen();
            output = &m_blended;
        }
    }

    m_post.apply(*output, outputSize, post, static_cast<float>(demoTime));
}

void Demo::renderEntry(int index, double localTime, double demoTime, RenderTarget& target) {
    const TimelineEntry& entry = m_timeline.entries()[index];
    target.bind();
    target.clear();

    const auto it = m_scenes.find(entry.scene);
    if (it == m_scenes.end() || !it->second) return;

    const SceneContext context{
        demoTime,
        localTime,
        entry.duration,
        static_cast<float>(std::clamp(localTime / entry.duration, 0.0, 1.0)),
        target.size(),
        entry.params,
        m_sync,
    };
    it->second->render(context, target);
}

ShaderProgram* Demo::transition(const std::string& name) {
    if (name == "none") return nullptr;
    auto it = m_transitions.find(name);
    if (it == m_transitions.end()) {
        // Loaded on first use; a missing file shows up as an error in the shader list and falls back to a cut.
        ShaderProgram& program = m_resources.shaders().loadFullscreen("transition/" + name, "transitions/" + name + ".frag");
        it = m_transitions.emplace(name, &program).first;
    }
    return it->second;
}

}  // namespace demo

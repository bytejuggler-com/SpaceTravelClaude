#include "dev/DevOverlay.h"

#include "core/App.h"
#include "core/DemoClock.h"
#include "core/Log.h"
#include "core/Shader.h"
#include "demo/Demo.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <cmath>
#include <format>

namespace demo {

namespace {

const ImVec4 kGreen{0.4f, 0.9f, 0.4f, 1.0f};
const ImVec4 kYellow{1.0f, 0.8f, 0.3f, 1.0f};
const ImVec4 kRed{1.0f, 0.4f, 0.4f, 1.0f};

}  // namespace

DevOverlay::DevOverlay(SDL_Window* window, SDL_GLContext context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // layout is set in code, no imgui.ini clutter

    m_scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
    if (m_scale <= 0.0f) m_scale = 1.0f;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.ScaleAllSizes(m_scale);

    ImFontConfig font;
    font.SizePixels = std::round(13.0f * m_scale);
    io.Fonts->AddFontDefault(&font);

    ImGui_ImplSDL3_InitForOpenGL(window, context);
    ImGui_ImplOpenGL3_Init("#version 460");
}

DevOverlay::~DevOverlay() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void DevOverlay::processEvent(const SDL_Event& event) { ImGui_ImplSDL3_ProcessEvent(&event); }

bool DevOverlay::wantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }

void DevOverlay::render(DemoClock& clock, ShaderLib& shaders, Demo& demo, const FrameStats& stats) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    drawDemoWindow(clock, demo, stats);
    drawTimelineWindow(clock, demo);
    drawShaderWindow(shaders);
    drawTweaksWindow(demo);
    drawLogWindow();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DevOverlay::drawDemoWindow(DemoClock& clock, Demo& demo, const FrameStats& stats) {
    const float pad = 10.0f * m_scale;
    ImGui::SetNextWindowPos({pad, pad}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({420.0f * m_scale, 0.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);
    if (ImGui::Begin("Demo")) {
        ImGui::Text("%.1f fps   CPU %.2f ms   GPU %.2f ms", stats.fps, stats.cpuMs, stats.gpuMs);
        ImGui::TextDisabled("%dx%d  %s", stats.width, stats.height, stats.glRenderer.c_str());
        ImGui::Separator();

        // The clock runs forever; the slider shows and sets the position within the current loop.
        const Playhead& playhead = demo.playhead();
        double time = playhead.timelineTime;
        const double minTime = 0.0, maxTime = demo.timeline().duration();
        ImGui::SetNextItemWidth(-60.0f * m_scale);
        if (ImGui::SliderScalar("time", ImGuiDataType_Double, &time, &minTime, &maxTime, "%.2f s"))
            clock.seek(playhead.lapStart + time);

        float speed = static_cast<float>(clock.speed());
        ImGui::SetNextItemWidth(-60.0f * m_scale);
        if (ImGui::SliderFloat("speed", &speed, 0.0f, 4.0f, "%.2fx")) clock.setSpeed(speed);

        bool paused = clock.paused();
        if (ImGui::Checkbox("Pause", &paused)) clock.setPaused(paused);
        ImGui::SameLine();
        if (ImGui::Button("Restart")) clock.seek(0.0);
        ImGui::SameLine();
        if (ImGui::Button("1x")) clock.setSpeed(1.0);

        ImGui::Separator();
        ImGui::TextDisabled("F1 overlay  F5 reload shaders+timeline  F11 fullscreen");
        ImGui::TextDisabled("F12 screenshot  Space pause  Left/Right seek 5 s");
        ImGui::TextDisabled("PgUp/PgDn prev/next entry  L loop entry  M mute");
        ImGui::TextDisabled("Esc quit");
    }
    ImGui::End();
}

void DevOverlay::drawTimelineWindow(DemoClock& clock, Demo& demo) {
    const float pad = 10.0f * m_scale;
    ImGui::SetNextWindowPos({pad, 230.0f * m_scale}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({420.0f * m_scale, 0.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);
    if (ImGui::Begin("Timeline")) {
        const auto& entries = demo.timeline().entries();
        const Playhead& playhead = demo.playhead();

        if (playhead.current >= 0) {
            const TimelineEntry& entry = entries[playhead.current];
            const float progress = static_cast<float>(playhead.localTime / entry.duration);
            const std::string label = std::format("{}  {:.1f} / {:.1f} s", entry.scene, playhead.localTime, entry.duration);
            ImGui::ProgressBar(progress, {-1.0f, 0.0f}, label.c_str());
            if (playhead.transitioning()) {
                const char* from = playhead.previous >= 0 ? entries[playhead.previous].scene.c_str() : "black";
                ImGui::Text("%s: %s -> %s  %.0f%%", entry.transition.c_str(), from, entry.scene.c_str(),
                            playhead.blend * 100.0f);
            } else {
                ImGui::TextDisabled("no transition running");
            }
        }

        auto& loop = demo.loopEntry();
        bool looping = loop.has_value();
        if (ImGui::Checkbox("Loop current entry (L)", &looping)) {
            if (looping && playhead.current >= 0) loop = playhead.current;
            else loop.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) demo.reloadTimeline();

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("entries", 4, flags)) {
            ImGui::TableSetupColumn("scene");
            ImGui::TableSetupColumn("start");
            ImGui::TableSetupColumn("length");
            ImGui::TableSetupColumn("transition");
            ImGui::TableHeadersRow();
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const TimelineEntry& entry = entries[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const std::string id = std::format("{}##entry{}", entry.scene, i);
                if (ImGui::Selectable(id.c_str(), i == playhead.current, ImGuiSelectableFlags_SpanAllColumns))
                    demo.seekToEntry(clock, i);
                if (!entry.params.values().empty() && ImGui::IsItemHovered()) {
                    std::string params;
                    for (const auto& [key, value] : entry.params.values()) params += std::format("{}={}\n", key, value);
                    ImGui::SetTooltip("%s", params.c_str());
                }
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", entry.start);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", entry.duration);
                ImGui::TableNextColumn();
                if (entry.transitionLength > 0.0)
                    ImGui::Text("%s %.1f", entry.transition.c_str(), entry.transitionLength);
                else
                    ImGui::TextDisabled("cut");
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void DevOverlay::drawShaderWindow(ShaderLib& shaders) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float pad = 10.0f * m_scale;
    const float width = 420.0f * m_scale;
    ImGui::SetNextWindowPos({display.x - width - pad, pad}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({width, 0.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);
    if (ImGui::Begin("Shaders")) {
        ImGui::TextDisabled("%s", shaders.root().string().c_str());
        if (ImGui::Button("Reload all (F5)")) shaders.reloadAll();
        ImGui::Separator();
        for (const auto& program : shaders.programs()) {
            const bool ok = program->lastError().empty();
            ImGui::TextColored(ok ? kGreen : kRed, ok ? "OK " : "ERR");
            ImGui::SameLine();
            ImGui::TextUnformatted(program->name().c_str());
            if (!ok) {
                ImGui::PushStyleColor(ImGuiCol_Text, kRed);
                ImGui::TextWrapped("%s", program->lastError().c_str());
                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::End();
}

void DevOverlay::drawTweaksWindow(Demo& demo) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float pad = 10.0f * m_scale;
    const float width = 420.0f * m_scale;
    ImGui::SetNextWindowPos({display.x - width - pad, 330.0f * m_scale}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({width, 0.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);
    if (ImGui::Begin("Tweaks")) {
        if (ImGui::CollapsingHeader("Music & sync", ImGuiTreeNodeFlags_DefaultOpen)) drawSyncSection(demo);
        if (ImGui::CollapsingHeader("Post (global defaults)")) {
            PostSettings& post = demo.postSettings();
            ImGui::SliderFloat("render scale", &demo.renderScale(), 0.25f, 1.0f, "%.2f");
            ImGui::SliderFloat("exposure", &post.exposure, 0.1f, 4.0f);
            ImGui::SliderFloat("bloom", &post.bloomIntensity, 0.0f, 3.0f);
            ImGui::SliderFloat("threshold", &post.bloomThreshold, 0.0f, 4.0f);
            ImGui::SliderFloat("knee", &post.bloomKnee, 0.0f, 1.0f);
            ImGui::SliderFloat("radius", &post.bloomRadius, 0.5f, 3.0f);
            ImGui::SliderFloat("chromatic", &post.chromatic, 0.0f, 5.0f);
            ImGui::SliderFloat("vignette", &post.vignette, 0.0f, 1.0f);
            ImGui::SliderFloat("grain", &post.grain, 0.0f, 0.2f);
            if (ImGui::Button("Reset post")) post = PostSettings{};
            ImGui::TextDisabled("Timeline post.<name>= params override these per entry.");
        }
        for (const auto& [name, scene] : demo.scenes()) {
            if (!scene) continue;
            if (ImGui::CollapsingHeader(name.c_str())) {
                ImGui::PushID(name.c_str());
                scene->imgui();
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}

void DevOverlay::drawSyncSection(Demo& demo) {
    MusicPlayer& music = demo.music();
    const SyncState& sync = demo.sync();

    if (music.loaded())
        ImGui::Text("%s  (%.1f s)", music.title().c_str(), music.duration());
    else
        ImGui::TextDisabled("no music (%s)", demo.timeline().music().c_str());
    if (!music.deviceOpen()) ImGui::TextColored(kYellow, "no audio device: running silent");

    bool muted = music.muted();
    if (ImGui::Checkbox("Mute (M)", &muted)) music.setMuted(muted);
    ImGui::SameLine();
    float volume = music.volume();
    ImGui::SetNextItemWidth(-60.0f * m_scale);
    if (ImGui::SliderFloat("volume", &volume, 0.0f, 1.0f)) music.setVolume(volume);

    float latencyMs = demo.audioLatency() * 1000.0f;
    ImGui::SetNextItemWidth(-60.0f * m_scale);
    if (ImGui::SliderFloat("latency", &latencyMs, 0.0f, 300.0f, "%.0f ms")) demo.audioLatency() = latencyMs / 1000.0f;
    ImGui::TextDisabled("Shifts visuals to match what you hear.");

    ImGui::Text("%.0f bpm   beat %d.%d   order %d  row %d", sync.bpm, static_cast<int>(sync.beat) / 4 + 1,
                static_cast<int>(sync.beat) % 4 + 1, sync.order, sync.row);

    // Pulse meters: watch them flash with the music to check the sync.
    auto meter = [this](const char* label, float value) {
        ImGui::ProgressBar(value, {40.0f * m_scale, 6.0f * m_scale}, "");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", label);
        ImGui::SameLine();
    };
    ImGui::TextUnformatted("beat");
    ImGui::SameLine();
    meter("beat", sync.beatPulse());
    ImGui::NewLine();
    ImGui::TextUnformatted("inst");
    ImGui::SameLine();
    for (int i = 1; i <= 8; ++i) meter(std::format("instrument {}", i).c_str(), sync.instrumentPulse(i));
    ImGui::NewLine();
    ImGui::TextUnformatted("chan");
    ImGui::SameLine();
    for (int c = 0; c < 8; ++c) meter(std::format("channel {}", c).c_str(), sync.channelPulse(c));
    ImGui::NewLine();
}

void DevOverlay::drawLogWindow() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float pad = 10.0f * m_scale;
    const ImVec2 size{display.x * 0.5f, display.y * 0.22f};
    ImGui::SetNextWindowPos({pad, display.y - size.y - pad}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.8f);
    if (ImGui::Begin("Log")) {
        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        for (const log::Entry& entry : log::recent()) {
            switch (entry.level) {
                case log::Level::Info: ImGui::TextUnformatted(entry.text.c_str()); break;
                case log::Level::Warn: ImGui::TextColored(kYellow, "%s", entry.text.c_str()); break;
                case log::Level::Error: ImGui::TextColored(kRed, "%s", entry.text.c_str()); break;
            }
        }
        if (atBottom) ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();
}

}  // namespace demo

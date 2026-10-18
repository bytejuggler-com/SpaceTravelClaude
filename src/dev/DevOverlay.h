#pragma once

#include <SDL3/SDL.h>

namespace demo {

class Demo;
class DemoClock;
class ShaderLib;
struct FrameStats;

// ImGui dev overlay (dev builds only): timings, clock + timeline control, shader status, post/scene tweaks, log.
class DevOverlay {
public:
    DevOverlay(SDL_Window* window, SDL_GLContext context);
    ~DevOverlay();
    DevOverlay(const DevOverlay&) = delete;
    DevOverlay& operator=(const DevOverlay&) = delete;

    void processEvent(const SDL_Event& event);
    bool wantsKeyboard() const;

    bool visible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    // Builds and draws the overlay on top of the current back buffer.
    void render(DemoClock& clock, ShaderLib& shaders, Demo& demo, const FrameStats& stats);

private:
    void drawDemoWindow(DemoClock& clock, Demo& demo, const FrameStats& stats);
    void drawTimelineWindow(DemoClock& clock, Demo& demo);
    void drawShaderWindow(ShaderLib& shaders);
    void drawTweaksWindow(Demo& demo);
    void drawSyncSection(Demo& demo);
    void drawLogWindow();

    float m_scale = 1.0f;
    bool m_visible = true;
};

}  // namespace demo

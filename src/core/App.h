#pragma once

#include "core/DemoClock.h"
#include "core/Gl.h"
#include "core/GpuTimer.h"
#include "core/Shader.h"

#include <SDL3/SDL.h>

#include <memory>
#include <optional>
#include <string>

namespace demo {

class Demo;
class DevOverlay;

struct AppOptions {
    int width = 0;   // 0 = fit 16:9 into the primary display
    int height = 0;
    bool fullscreen = false;  // distribution builds default to true (see main.cpp)
    bool vsync = true;
    bool mute = false;
    std::optional<std::string> capturePath;  // render one frame at captureTime, save PNG, quit
    double captureTime = 0.0;
    bool captureOverlay = false;  // include the dev overlay in the capture
    double quitAfter = 0.0;       // wall-clock seconds; 0 = run until closed
};

struct FrameStats {
    double fps = 0.0;
    double cpuMs = 0.0;
    double gpuMs = 0.0;
    int width = 0;
    int height = 0;
    std::string glRenderer;
};

class App {
public:
    explicit App(AppOptions options);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

private:
    void initWindow();
    void initGl();
    void handleEvent(const SDL_Event& event);
    void toggleFullscreen();

    AppOptions m_options;
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_running = true;
    bool m_screenshotRequested = false;

    DemoClock m_clock;
    FrameStats m_stats;
    std::unique_ptr<GpuTimer> m_gpuTimer;
    std::unique_ptr<ShaderLib> m_shaders;
    std::unique_ptr<Demo> m_demo;
#ifdef DEMO_DEV_MODE
    std::unique_ptr<DevOverlay> m_overlay;
#endif
};

}  // namespace demo

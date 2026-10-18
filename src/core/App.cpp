#include "core/App.h"

#include "core/GlDebug.h"
#include "core/Log.h"
#include "core/Screenshot.h"
#include "demo/Demo.h"
#ifdef DEMO_DEV_MODE
#include "dev/DevOverlay.h"
#endif

#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace demo {

namespace {

constexpr int kCaptureWarmupFrames = 3;  // let ImGui lay out and the driver settle before capturing

[[noreturn]] void fail(const std::string& what) {
    throw std::runtime_error(std::format("{}: {}", what, SDL_GetError()));
}

// Folder holding shaders/ and data/. Dev builds use the source tree so edits hot-reload in place.
fs::path findAssetRoot() {
    fs::path sourceTree = DEMO_ASSET_DIR;
#ifdef DEMO_DEV_MODE
    if (fs::exists(sourceTree / "shaders")) return sourceTree;
#endif
    const char* base = SDL_GetBasePath();
    fs::path besideExe = fs::path(base ? base : "");
    return fs::exists(besideExe / "shaders") ? besideExe : sourceTree;
}

}  // namespace

App::App(AppOptions options) : m_options(std::move(options)) {
    initWindow();
    initGl();

    const fs::path assets = findAssetRoot();
    m_shaders = std::make_unique<ShaderLib>(assets / "shaders");
    m_demo = std::make_unique<Demo>(*m_shaders, assets / "data" / "demo.timeline");
    m_demo->music().setMuted(m_options.mute);

#ifdef DEMO_DEV_MODE
    m_overlay = std::make_unique<DevOverlay>(m_window, m_glContext);
    if (m_options.capturePath) m_overlay->setVisible(m_options.captureOverlay);
#endif

    if (m_options.capturePath) {
        m_clock.seek(m_options.captureTime);
        m_clock.setPaused(true);
    }
}

App::~App() {
    // GL objects must go before the context.
#ifdef DEMO_DEV_MODE
    m_overlay.reset();
#endif
    m_demo.reset();
    m_shaders.reset();
    m_gpuTimer.reset();

    if (m_glContext) SDL_GL_DestroyContext(m_glContext);
    if (m_window) SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void App::initWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO)) fail("SDL_Init failed");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) log::warn("SDL audio init failed ({}); running silent", SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef DEMO_DEV_MODE
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);  // scenes render into their own FBOs

    int width = m_options.width;
    int height = m_options.height;
    if (width <= 0 || height <= 0) {
        // Fit a 16:9 window into 80% of the primary display's usable area.
        SDL_Rect usable{0, 0, 1280, 720};
        SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable);
        width = std::min(usable.w * 8 / 10, (usable.h * 8 / 10) * 16 / 9);
        height = width * 9 / 16;
    }

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (m_options.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    m_window = SDL_CreateWindow("Space Travel", width, height, flags);
    if (!m_window) fail("SDL_CreateWindow failed");
    if (m_options.fullscreen) SDL_HideCursor();
}

void App::initGl() {
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) fail("OpenGL 4.6 core context creation failed");
    SDL_GL_MakeCurrent(m_window, m_glContext);
    SDL_GL_SetSwapInterval(m_options.vsync ? 1 : 0);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
        throw std::runtime_error("Failed to load OpenGL functions");
    if (GLVersion.major < 4 || (GLVersion.major == 4 && GLVersion.minor < 6))
        throw std::runtime_error(std::format("OpenGL 4.6 required, got {}.{}", GLVersion.major, GLVersion.minor));

    m_stats.glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    log::info("OpenGL {} on {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)), m_stats.glRenderer);
    installGlDebugOutput();

    m_gpuTimer = std::make_unique<GpuTimer>();
}

int App::run() {
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const Uint64 start = SDL_GetPerformanceCounter();
    Uint64 previous = start;
    int frame = 0;

    while (m_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handleEvent(event);

        const Uint64 now = SDL_GetPerformanceCounter();
        const double wallDt = static_cast<double>(now - previous) / freq;
        previous = now;
        m_clock.advance(wallDt);

        // Smoothed frame stats for the overlay.
        const double smoothing = frame == 0 ? 1.0 : 0.05;
        m_stats.cpuMs += (wallDt * 1000.0 - m_stats.cpuMs) * smoothing;
        m_stats.fps = m_stats.cpuMs > 0.0 ? 1000.0 / m_stats.cpuMs : 0.0;
        m_stats.gpuMs = m_gpuTimer->lastMs();

#ifdef DEMO_DEV_MODE
        m_shaders->pollHotReload();
#endif
        m_demo->update(m_clock);

        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
        m_stats.width = width;
        m_stats.height = height;

        m_gpuTimer->begin();
        m_demo->render(m_clock.time(), {width, height});
        m_gpuTimer->end();

#ifdef DEMO_DEV_MODE
        if (m_overlay->visible()) m_overlay->render(m_clock, *m_shaders, *m_demo, m_stats);
#endif

        if (m_options.capturePath && frame + 1 == kCaptureWarmupFrames) {
            saveScreenshot(*m_options.capturePath, width, height);
            m_running = false;
        } else if (m_screenshotRequested) {
            saveScreenshot(std::format("screenshot_{}.png", SDL_GetTicks()), width, height);
            m_screenshotRequested = false;
        }

        SDL_GL_SwapWindow(m_window);
        ++frame;

        if (m_options.quitAfter > 0.0 && static_cast<double>(now - start) / freq >= m_options.quitAfter)
            m_running = false;
    }
    return 0;
}

void App::handleEvent(const SDL_Event& event) {
    bool overlayHasKeyboard = false;
#ifdef DEMO_DEV_MODE
    m_overlay->processEvent(event);
    overlayHasKeyboard = m_overlay->visible() && m_overlay->wantsKeyboard();
#endif

    if (event.type == SDL_EVENT_QUIT) {
        m_running = false;
        return;
    }
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;

    const SDL_Keycode key = event.key.key;
#ifdef DEMO_DEV_MODE
    if (key == SDLK_F1) {
        m_overlay->setVisible(!m_overlay->visible());
        return;
    }
    if (key == SDLK_F5) {
        m_shaders->reloadAll();
        m_demo->reloadTimeline();
        return;
    }
#endif
    if (overlayHasKeyboard) return;

    const int entry = m_demo->playhead().current;
    if (key == SDLK_ESCAPE) m_running = false;
    else if (key == SDLK_F11) toggleFullscreen();
    else if (key == SDLK_F12) m_screenshotRequested = true;
    else if (key == SDLK_SPACE) m_clock.setPaused(!m_clock.paused());
    else if (key == SDLK_LEFT) m_clock.seek(m_clock.time() - 5.0);
    else if (key == SDLK_RIGHT) m_clock.seek(m_clock.time() + 5.0);
    else if (key == SDLK_PAGEUP) m_demo->seekToEntry(m_clock, entry - 1);
    else if (key == SDLK_PAGEDOWN) m_demo->seekToEntry(m_clock, entry + 1);
    else if (key == SDLK_M) m_demo->music().setMuted(!m_demo->music().muted());
    else if (key == SDLK_L) {
        auto& loop = m_demo->loopEntry();
        if (loop) loop.reset();
        else if (entry >= 0) loop = entry;
    }
}

void App::toggleFullscreen() {
    const bool fullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(m_window, !fullscreen);
    if (fullscreen) SDL_ShowCursor();  // leaving fullscreen
    else SDL_HideCursor();
}

}  // namespace demo

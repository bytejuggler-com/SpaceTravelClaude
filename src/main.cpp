#include "core/App.h"
#include "core/Log.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

namespace {

constexpr const char* kUsage =
    "SpaceTravel [options]\n"
    "  --width N --height N   window size (default: fit 16:9 into the display)\n"
    "  --fullscreen           start fullscreen (default in distribution builds; F11 toggles)\n"
    "  --windowed             start in a window\n"
    "  --no-vsync             disable vsync\n"
    "  --mute                 start with music muted (M toggles)\n"
    "  --capture FILE.png     render one frame, save it and quit\n"
    "  --time SECONDS         demo time for --capture\n"
    "  --overlay              include the dev overlay in --capture\n"
    "  --quit-after SECONDS   quit after this much wall-clock time";

void printUsage() {
#ifdef DEMO_DEV_MODE
    std::puts(kUsage);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Space Travel", kUsage, nullptr);  // no console here
#endif
}

bool parseArgs(int argc, char* argv[], demo::AppOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--fullscreen") options.fullscreen = true;
        else if (arg == "--windowed") options.fullscreen = false;
        else if (arg == "--no-vsync") options.vsync = false;
        else if (arg == "--overlay") options.captureOverlay = true;
        else if (arg == "--mute") options.mute = true;
        else if (arg == "--width" && hasValue) options.width = std::atoi(argv[++i]);
        else if (arg == "--height" && hasValue) options.height = std::atoi(argv[++i]);
        else if (arg == "--capture" && hasValue) options.capturePath = argv[++i];
        else if (arg == "--time" && hasValue) options.captureTime = std::atof(argv[++i]);
        else if (arg == "--quit-after" && hasValue) options.quitAfter = std::atof(argv[++i]);
        else return false;
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    demo::AppOptions options;
#ifndef DEMO_DEV_MODE
    options.fullscreen = true;  // a demo starts fullscreen; --windowed opts out
#endif
    if (!parseArgs(argc, argv, options)) {
        printUsage();
        return 2;
    }

    try {
        demo::App app(options);
        return app.run();
    } catch (const std::exception& e) {
        demo::log::error("{}", e.what());
        const bool unattended = options.capturePath || options.quitAfter > 0.0;
        if (!unattended) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Space Travel", e.what(), nullptr);
        return 1;
    }
}

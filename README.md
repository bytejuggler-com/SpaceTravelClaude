# Space Travel Demo

Old-school demoscene-style space travel in C++20 + OpenGL 4.6: an Amiga-style intro, parallax and warp
starfields, a giant star with corona flames, an asteroid chase, a capital ship in orbit, a spiral galaxy and a
black hole, all procedural and synced to tracker music. See [PLAN.md](PLAN.md) for the design and roadmap.

## Build

Requires Visual Studio 2022 (Desktop C++ workload). CMake and vcpkg bundled with VS are used automatically.

```powershell
.\build.ps1                  # Debug, with the dev overlay and hot-reload
.\build.ps1 -Config release  # optimized, still with dev features
.\build.ps1 -Config dist     # distribution: no dev features, static runtime and libraries
.\build.ps1 -Run             # build and launch
```

The first build of each configuration pins the vcpkg registry baseline and compiles the dependencies
(SDL3, glad, glm, Dear ImGui, stb, libopenmpt), which takes a few minutes.
You can also open the folder in Visual Studio and pick the `vs2022` or `vs2022-dist` preset.

## Package

```powershell
.\package.ps1        # dist\SpaceTravel\ : SpaceTravel.exe + shaders\ + data\ + README.txt
.\package.ps1 -Zip   # also dist\SpaceTravel.zip
```

The packaged folder runs on any Windows 10/11 x64 PC with OpenGL 4.6: the exe is statically linked
(no Visual C++ runtime or DLLs needed), starts fullscreen with the cursor hidden, and loops forever.

## Run

Dev build: `build\vs2022\Debug\SpaceTravel.exe` (windowed by default).

| Key | Action | Builds |
|-----|--------|--------|
| F1 | Toggle dev overlay | dev |
| F5 | Reload all shaders and the timeline | dev |
| L | Loop the current timeline entry | all |
| F11 | Toggle fullscreen | all |
| F12 | Save screenshot | all |
| Space | Pause | all |
| Left / Right | Seek ±5 s | all |
| PgUp / PgDn | Previous / next timeline entry | all |
| M | Mute music | all |
| Esc | Quit | all |

Command line: `--width N --height N --fullscreen --windowed --no-vsync --mute --capture out.png --time 12.5 --overlay --quit-after 10`

## Music

Tracker modules (.mod, .xm, .s3m, .it, ...) play through libopenmpt. Until a real track is chosen, a placeholder
ProTracker song is generated in code. To use your own, put it in `data/music/` and set `@music music/<file>` and
`@bpm <tempo>` in the timeline. Durations like `16b` are in beats, so entries line up with the music.
Scenes get the beat grid and per-instrument note-on timing; the dev overlay's "Music & sync" panel shows
live pulse meters to check the sync and a latency slider to line visuals up with what you hear.

## Timeline

The scene sequence lives in [data/demo.timeline](data/demo.timeline) (format documented at the top of the file).
Dev builds reload it when saved. To add a scene, see "Adding a new scene" in [PLAN.md](PLAN.md).

## Shaders

GLSL lives in `shaders/`. `#include "common/noise.glsl"` paths are relative to that folder.
Dev builds read shaders straight from the source tree and rebuild any program whose files change, so edits show up live.
If an edit doesn't compile, the previous version keeps running and the error shows in the overlay.

## Documentation

- [docs/artbook/SpaceTravel-ArtBook.pdf](docs/artbook/SpaceTravel-ArtBook.pdf): the printed art book (built from `artbook.html`
  with headless Edge).
- [docs/explainer/index.html](docs/explainer/index.html): one interactive page per scene. Each runs the scene's real shaders
  in WebGL2 and explains the techniques from first principles with live figures, pseudo-code and the actual source. Every
  page is a single self-contained file that works offline.

## License

- **Code** (everything outside `docs/`): [MIT License](LICENSE).
- **Documentation** (`docs/`: art book, images, explainer text): [Creative Commons Attribution 4.0](docs/LICENSE.md) (CC BY 4.0).

Copyright (c) 2026 BlackLizard (bytejuggler.com). Third-party libraries fetched at build time (SDL3, libopenmpt, Dear ImGui, glm, glad, stb) keep their own licenses.

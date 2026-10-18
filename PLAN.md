# Space Travel Demo — Plan

An old-school (Amiga/PC demoscene style) sequence of space-travel effects. "Coolness" and
technical flair over physical accuracy. Scenes play in sequence, a few seconds each, synced
to tracker music.

## Decisions

| Topic   | Decision |
|---------|----------|
| Stack   | C++20 + OpenGL 4.6 core profile, GLSL shaders |
| Window / input / audio out | SDL3 |
| GL loader | glad (GL 4.6 core) |
| Math    | glm |
| Dev UI  | Dear ImGui (dev builds only) |
| Music   | Tracker module (.mod/.xm/.it/.s3m) via libopenmpt, beat/row sync |
| Look    | Modern HDR rendering (bloom, volumetrics) with demo pacing, scrollers, transitions |
| Assets  | Fully procedural; only external files are the music module (font is a built-in 8x8 bitmap) |
| Build   | CMake + vcpkg manifest mode, Visual Studio 2022 (MSVC) |
| Target HW | AMD Radeon 880M (iGPU) at 1080p60 — heavy raymarched passes render at 1/2–1/3 res and upscale |

## Architecture

```
App
├── Core
│   ├── Window + GL 4.6 context (KHR_debug output in dev builds)
│   ├── DemoClock   — seekable demo time (wall clock); the music follows it and re-seeks on drift
│   ├── ShaderLib   — GLSL loader with #include, hot-reload on file change (dev), embedded in release
│   ├── RenderTarget — RAII FBO + color texture (+ optional depth), GPU timer queries
│   └── Resources   — ShaderLib access, fullscreen triangle (later: shared noise textures)
├── Scene system
│   ├── Scene interface (init / render / imgui)
│   ├── SceneRegistry — REGISTER_SCENE("name", Class) self-registration
│   └── Timeline     — data/demo.timeline: scene, duration, transition, params; loops; hot-reloads in dev
├── Demo        — plays the timeline: renders current (+ previous during transitions) into HDR targets
├── Transitions — one shader per file in shaders/transitions/: crossfade, flash, wipe, dissolve, glitch
├── Post chain  — HDR → bloom (13-tap down / tent up mip chain) → chromatic aberration → ACES tonemap → vignette/grain → sRGB
├── Audio       — libopenmpt → SDL3 audio stream; SyncTrack scans the module once for row/note-on times;
│                 SyncState (beat, per-channel / per-instrument "seconds since hit") is a pure function of time
└── Dev overlay — scene list, jump/loop/pause/scrub, live param sliders, per-pass GPU timings, FPS
```

### Scene contract

```cpp
struct SceneContext {
    double demoTime;     // global time (s)
    double localTime;    // time since the timeline entry started (s)
    double duration;     // entry length (s)
    float  progress;     // 0..1 across the entry
    glm::ivec2 size;     // render target size
    const SceneParams& params;   // key=value numbers from the timeline line
    const SyncState& sync;       // beat, order/row, sync.beatPulse(), sync.instrumentPulse(n), sync.channelPulse(c)
};

class Scene {
public:
    virtual ~Scene() = default;
    virtual void init(Resources&) = 0;                                 // load shaders / allocate once
    virtual void render(const SceneContext&, RenderTarget& hdrOut) = 0;  // target bound + cleared on entry
    virtual void imgui() {}                                            // dev-only tweakables
};
```

Scenes are a pure function of the context (no state accumulated across frames, no `update()`):
one instance serves every timeline entry that names it and may render twice per frame during a transition.
That keeps the demo seekable, loopable and scrubbable. Particle effects (warp, galaxy) compute positions
analytically from time instead of integrating.

### Adding a new scene

1. `src/scenes/<name>/<Name>Scene.cpp` implementing `Scene` + `REGISTER_SCENE("<name>", ...)`
   (picked up by a CMake glob, re-run configure)
2. Shaders in `shaders/scenes/<name>/`
3. One line in `data/demo.timeline`

Adding a transition = dropping `shaders/transitions/<name>.frag` (interface in `shaders/common/transition.glsl`).

### Timeline format

```
# scene    duration  [transition [length]]  [key=value ...]
intro      10.0
hstars      8.0      flash      0.4
warp       10.0      crossfade  1.0        speed=3.0
corona     12.0      dissolve   1.0        post.bloom=1.2
galaxy     14.0      crossfade  1.5
```

The transition blends in from the previous entry during the first `length` seconds of the entry;
the first entry fades in from black and, on later loops, from the last entry.
`post.<name>=value` overrides post settings (exposure, bloom, threshold, knee, radius, chromatic, vignette, grain),
interpolated across transitions.

## Project layout

```
CMakeLists.txt, CMakePresets.json, vcpkg.json
src/
  core/        app, window, clock, gl wrappers, shader lib
  audio/       openmpt player, sync
  scene/       scene interface, registry, timeline
  demo/        timeline playback, transitions
  post/        bloom, tonemap, fx
  scenes/      one folder per scene
  dev/         imgui overlay
shaders/
  common/      noise.glsl (value/simplex/fbm/curl), math.glsl, color.glsl, sdf.glsl
  post/, transitions/, scenes/<name>/
data/
  demo.timeline
  music/       tracker module(s)
```

## Initial scenes

| # | Scene | Technique |
|---|-------|-----------|
| 1 | **Intro** — title logo, copper bars, sine scroller | One fragment shader; built-in 8x8 bitmap font (R8 atlas + R8UI text texture); chrome logo with bounce-in, row wobble and shine; z-sorted copper bars flashing on the kick; per-column sine scroller with water reflection |
| 2 | **Horizontal starfield** — Amiga multi-layer parallax | Procedural quads from gl_VertexID, 6 depth layers, speed/size/brightness by layer, motion streaks; gas giant (raymarched-style sphere shading, fbm bands) between far and near layers; accelerates at the end |
| 3 | **3D warp starfield** — accelerating to hyperspace | 60k+ stars computed analytically in the vertex shader from the integrated travel distance (scrubbable, no compute state); head-to-tail streak quads; camera roll; blue shift and hyperspace tunnel glow; flash transition out |
| 4 | **Giant star patrol** — orbiting a star with corona flames | Ray-sphere star at 1/2 res: domain-warped fbm granulation, fine cells, sunspots, limb darkening; corona streamers evaluated at the ray's closest approach (noise domain shifted by height − time so flames flow outward); 5 prominence arcs raymarched as glowing torus-segment tubes; full-res celestial-sphere star sky composited behind; orbit camera with close fly-by; corona flares on the snare |
| 5 | **Black hole** — relativistic view, attraction kept in the center | Fullscreen, corona-style pipeline (1/2-res main pass + full-res sky). Rays bent step by step (Schwarzschild photon-path approximation) → shadow, photon ring, far side of the disk arching over/under the shadow, Einstein-ring lensing of a bright nebula band placed behind. Thin disk: temperature profile, turbulent spiral streaks with Keplerian differential rotation, Doppler beaming + blueshift, gravitational redshift near the inner edge. Slow inclined camera with gradual zoom; hot spots near the inner edge pulse with the music; optional polar jets |
| 6 | **Asteroid chase** — small ship weaving through an asteroid field, small star in a corner | Rasterized: ~8 procedural rock meshes (displaced icospheres with craters), thousands of instances. The flight path is built from the field: it skims one large "hero" rock after another, each waypoint set back from its rock along the light direction so the rock sits between ship and star and its shadow sweeps the hull; a smooth Hermite curve joins the waypoints and other rocks keep out of the corridor. Single harsh key light + near-zero ambient; cascaded soft shadow maps (rocks shadow each other and the ship); crater/grit detail from noise in the fragment shader; rim light; light shafts from the star through the field; lens flare; engine glow + trail; dust streaks. Ship banks with path curvature; chase camera trails the ship's past position with spring overshoot and shake |
| 7 | **Capital ship patrol** — wedge-shaped dreadnought orbiting a planet | Original design evoking the classic wedge silhouette. Procedural tiered hull, command tower + domes, engine block with big glowing nozzles, trenches. "City" of thousands of instanced blocks placed by rules. Cold metal (GGX), panel/plate variation from noise, high-res self-shadowing, SSAO for the crevices, window lights on the shadow side, environment reflections of planet + sun, blue planet bounce light. Planet: atmosphere rim, clouds, terminator, night-side city lights. Scripted cinematic shots (under the hull, along the city to the tower, engines from behind, wide with planet) |
| 8 | **Galaxy** — dynamic spiral galaxy fly-by | ~800k point-sprite stars on density-wave orbits (ellipses whose orientation twists with radius; positions analytic from id + time via PCG hashes, scrubbable); warm bulge, blue young disc stars, rare giants; pink/blue gas puffs added, dust puffs multiplied in (blend ZERO, ONE_MINUS_SRC_COLOR) trailing the arms; background pass with disc-plane haze and flattened core glow; camera orbit then dive into the disc toward the core |

### Scene parameters (planned, all usable as timeline params and overlay sliders)

- **blackhole**: `incl` (camera inclination), `zoom`, `inner` / `outer` disk radius, `temp`, `brightness`,
  `doppler` (0 = cinematic symmetric, 1 = physical beaming), `lensing`, `jets`, `quality` (ray steps)
- **asteroids**: `lag`, `shake`, `nearmiss`, `density`, `size`, `ambient` (shadow darkness), `softness`,
  `shafts`, `flare`, star position, `speed`, `bank`
- **dreadnought**: `sun`, `softness`, `ao`, `reflect`, `greebles`, `windows`, `engines`, `atmosphere`, `shot`

### Backlog (future scenes)

- Volumetric nebula fly-through
- Wormhole / hyperspace tunnel
- Planet with atmospheric scattering and rings (partly covered by the dreadnought's planet)
- End-credits scroller + greetings

## Phases

| Phase | Content | Done when |
|-------|---------|-----------|
| 0 ✅ | Skeleton: CMake + vcpkg, SDL3 window, GL 4.6 context, debug output, clock, shader hot-reload, ImGui | Window shows a test shader; editing the `.glsl` updates live |
| 1 ✅ | Framework: Scene interface + registry, timeline, HDR FBOs, transitions, post chain | Two dummy scenes cycle with transitions and bloom |
| 2 ✅ | Audio + sync: libopenmpt playback following the demo clock, beat grid + note-on events | Dummy scene pulses on the beat; scrubbing stays in sync |
| 3 ✅ | Scenes 1–3: intro scroller, horizontal starfield, 3D warp | Plays first ~30 s of the demo |
| 4 ✅ | Scene 4: star corona | Runs ≥60 fps on the 880M |
| 5 ✅ | Scene 5: black hole (fullscreen, no new engine work) | Lensing, photon ring and Doppler disk at ≥60 fps; parameters live |
| 6 🔨 | 3D renderer: procedural meshes, instancing, depth targets, cascaded soft shadow maps, shared lighting GLSL, light shafts, lens flare — first used by the asteroid field with a simple ship | Rocks shadow each other at ≥60 fps |
| 7 ⏸ | (deferred, user moved on to the dreadnought; the rock-skimming shadow path was done in Phase 6) Asteroid chase complete: ship model, chase camera (lag, shake, near misses), banking, engine trail, dust, shadow-band flicker | Scene feels like a pursuit |
| 8 🔨 | Dreadnought I: hull, tower, engines, metal shading, high-res self-shadows, cinematic shot list | Silhouette and lighting read well |
| 9 🔨 | Dreadnought II: greeble city, SSAO, window lights, planet with atmosphere + night lights, environment reflections | "City on the hull" look |
| 9b 🔨 | Dreadnought patrol: the ship moves, the tiny camera hangs or drifts (overhead pass, planet eclipse, deck drift, departure), ~9.6 s per shot over an 80-beat entry; lower bloom/sun for detail; randomized underside and plating | Patrol feels majestic |

Phase 9 so far: city blocks (noise districts, rows, towers, stacks, flat-footing rule), armored rims, ~80 turrets,
side-wall windows + hangars, underside plates + docking bay, tower and engine details, baked per-vertex contact
occlusion (new `Vertex::occlusion`, attribute 3). Planet night lights done in Phase 8. Still open: SSAO.
| 10 🔨 | Scene: galaxy (placed right before the black hole: dive toward the core, arrive at the hole) | Runs ≥60 fps on the 880M |
| 12–18 🔨 | Second wave: pillars, rings, wormhole, supernova, canyon, fleet, credits (table above) — all built and in the timeline (~5 min loop), untested at runtime | Each runs ≥60 fps on the 880M |
| 11 🔨 | Polish: final timeline, distribution build (`vs2022-dist` preset: dev features off, static CRT + x64-windows-static deps, WIN32 subsystem, fullscreen + hidden cursor by default), `package.ps1` → `dist\SpaceTravel\` (exe + shaders + data + README). Shaders stay as files next to the exe (moddable) rather than embedded. Still open: real music track, perf pass | Single folder runnable on another Windows PC |

3D renderer pieces (src/gfx): `MeshData` + generators (icosphere, loft, asteroid), `Mesh` (GPU, instanced
draws with baseInstance), `StorageBuffer` (per-frame instance data, binding 0, read by `shaders/common/mesh.vert`),
`ShadowMap` (depth texture array, hardware compare), `Frustum`; GLSL `common/lighting.glsl` (cascaded Poisson
PCF shadows, tangent-free bump mapping, specular, Fresnel). Scene render targets can carry a sampleable depth texture.

### Second wave of scenes (Phases 12–18, implemented in this order)

| Phase | Scene | Technique |
|-------|-------|-----------|
| 12 | **pillars** — nebula crossing among towering dust pillars | Half-res raymarched volume: dust pillars (tapered bent columns eroded by 3D noise) absorbing and scattering light from a star cluster behind them (shadow march toward the light, Henyey-Greenstein phase → volumetric light shafts, bright ionization rims), glowing Hubble-palette gas around; full-res star sky and cluster glow composited by transmittance |
| 13 | **rings** — ringed gas giant, diving through the ring plane | Raytraced planet + ring disc (banded profile with gaps), planet shadow on the rings, ring shadow bands on the planet, forward-scattering glow; near the camera the rings become instanced icy rocks (mesh renderer, depth-composited against the raytraced pass via gl_FragDepth) |
| 14 | **wormhole** — approach and plunge | Analytic lens: rays outside the mouth bend around it (our sky), rays inside map to a second sky (a face-on spiral galaxy: the next destination); volumetric gas swirling at the mouth; plunge into a streaking tunnel and out the other side |
| 15 | **supernova** — a star dies | Pulsating red giant (corona-style surface), collapse, flash on the beat, then a half-res raymarched turbulent shock shell (hot blue front, cooling pink filaments from cell noise), equatorial ring flash, debris streaks |
| 16 | **canyon** — low flight through an alien canyon at dusk | Raymarched heightfield (ridged terrain carved by a winding canyon, stratified walls), soft terrain shadows, low dust fog, aerial perspective, ringed planet in the sky; the fighter mesh depth-composited on top with its shadow sliding over the rocks |
| 17 | **fleet** — dreadnoughts dropping out of hyperspace | Instanced dreadnought meshes arriving on the beat with warp streaks and flashes into formation over the planet, shadowed |
| 18 | **credits** — vector balls and scroller | Instanced shiny spheres morphing between shapes (sphere, cube lattice, torus knot, text) on bar lines; credits and greetings in the 8x8 font scrolling over a starfield |

Shared for the volumetric scenes: `Resources::noiseVolume()`, a 128³ tileable RG8 texture (R: value-noise fbm,
G: inverted Worley fbm) generated once by `shaders/common/noise3d.comp`.

The 3D renderer (Phase 6) keeps the core rule: every mesh instance's transform is a function of time,
so scrubbing and looping still work. Meshes are generated in code at startup (no model files).

## Defaults (change if needed)

- 1920x1080 window, F11 toggles fullscreen, Esc quits, loops forever
- Dev build: ImGui overlay on F1, shader hot-reload; release build: neither
- Music: until a module is chosen, a placeholder 4-channel ProTracker song is generated in code
  (src/audio/PlaceholderSong.cpp, 125 BPM, 46 s). A real track goes in data/music/ and is selected with
  `@music music/<file>` in the timeline; it must have a license that allows reuse
  (e.g. Mod Archive tracks marked as such, or the author's permission).

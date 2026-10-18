#pragma once

#include "audio/SyncState.h"
#include "core/RenderTarget.h"

#include <glm/glm.hpp>

#include <map>
#include <string>
#include <string_view>

namespace demo {

class Resources;

// key=value numbers from a timeline entry, e.g. "speed=2.5 post.exposure=1.2".
class SceneParams {
public:
    void set(std::string key, float value) { m_values[std::move(key)] = value; }

    float get(std::string_view key, float fallback) const {
        const auto it = m_values.find(key);
        return it == m_values.end() ? fallback : it->second;
    }

    const std::map<std::string, float, std::less<>>& values() const { return m_values; }

private:
    std::map<std::string, float, std::less<>> m_values;
};

struct SceneContext {
    double demoTime;   // global demo time (s)
    double localTime;  // time since this timeline entry started (s); keeps running past `duration` while transitioning out
    double duration;   // length of this timeline entry (s)
    float progress;    // localTime / duration, clamped to [0, 1]
    glm::ivec2 size;   // render target size in pixels
    const SceneParams& params;
    const SyncState& sync;  // beat grid and note-on timing of the music
};

// One visual effect. The same instance serves every timeline entry that names it, and may be rendered twice
// in one frame during a transition, so render() should be a pure function of the context (no state
// accumulated across frames). That keeps the demo seekable, loopable and scrubbable.
class Scene {
public:
    virtual ~Scene() = default;

    // Called once, before the first render. Load shaders, allocate buffers.
    virtual void init(Resources& resources) = 0;

    // `target` is bound and cleared to black on entry; output linear HDR color into it.
    // If a scene binds other framebuffers it must re-bind `target` before drawing into it,
    // and must leave blending/depth testing disabled when it returns.
    virtual void render(const SceneContext& context, RenderTarget& target) = 0;

    // Dev-overlay tweakables (ImGui widgets).
    virtual void imgui() {}
};

}  // namespace demo

// Placeholder scene: drifting domain-warped nebula over parallax star layers (the Phase 0 test shader, now HDR).
// Timeline params: speed (default 1), pulse = how much the core glow throbs on the beat (default 1).

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

namespace demo {

namespace {

class NebulaScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        m_shader = &resources.shaders().loadFullscreen("scene/nebula", "scenes/nebula/nebula.frag");
    }

    void render(const SceneContext& context, RenderTarget& /*target*/) override {
        if (!m_shader->valid()) return;
        m_shader->use();
        m_shader->set("uTime", static_cast<float>(context.localTime * context.params.get("speed", 1.0f)));
        m_shader->set("uResolution", glm::vec2(context.size));
        m_shader->set("uWarp", m_warp);
        m_shader->set("uStarBrightness", m_starBrightness);
        m_shader->set("uGlow", m_glow);
        m_shader->set("uBeatPulse", context.sync.beatPulse(6.0f) * context.params.get("pulse", 1.0f));
        m_resources->drawFullscreen();
    }

    void imgui() override {
        ImGui::SliderFloat("warp", &m_warp, 0.0f, 6.0f);
        ImGui::SliderFloat("star brightness", &m_starBrightness, 0.0f, 10.0f);
        ImGui::SliderFloat("core glow", &m_glow, 0.0f, 10.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_shader = nullptr;
    float m_warp = 3.0f;
    float m_starBrightness = 4.0f;
    float m_glow = 3.0f;
};

}  // namespace

REGISTER_SCENE("nebula", NebulaScene);

}  // namespace demo

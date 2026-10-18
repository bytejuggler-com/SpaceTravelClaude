// Placeholder scene: outrun-style neon grid racing toward a striped sunset. Very bright lines to exercise bloom.
// The grid advances one cell per beat (times speed), lines flare on the kick and the sun on the snare.
// Timeline params: speed (default 1), kick / snare = instrument numbers in the module (default 1 / 2).

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

namespace demo {

namespace {

class NeonGridScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        m_shader = &resources.shaders().loadFullscreen("scene/neongrid", "scenes/neongrid/neongrid.frag");
    }

    void render(const SceneContext& context, RenderTarget& /*target*/) override {
        if (!m_shader->valid()) return;
        m_shader->use();
        m_shader->set("uTime", static_cast<float>(context.localTime));
        m_shader->set("uResolution", glm::vec2(context.size));
        const float speed = context.params.get("speed", 1.0f) * m_speed;
        const int kick = static_cast<int>(context.params.get("kick", 1.0f));
        const int snare = static_cast<int>(context.params.get("snare", 2.0f));
        m_shader->set("uScroll", static_cast<float>(context.sync.beat * speed));
        m_shader->set("uKick", context.sync.instrumentPulse(kick, 7.0f));
        m_shader->set("uSnare", context.sync.instrumentPulse(snare, 10.0f));
        m_shader->set("uSunSize", m_sunSize);
        m_shader->set("uGridGlow", m_gridGlow);
        m_resources->drawFullscreen();
    }

    void imgui() override {
        ImGui::SliderFloat("speed", &m_speed, 0.0f, 5.0f);
        ImGui::SliderFloat("sun size", &m_sunSize, 0.05f, 0.4f);
        ImGui::SliderFloat("grid glow", &m_gridGlow, 0.0f, 10.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_shader = nullptr;
    float m_speed = 1.0f;
    float m_sunSize = 0.22f;
    float m_gridGlow = 3.0f;
};

}  // namespace

REGISTER_SCENE("neongrid", NeonGridScene);

}  // namespace demo

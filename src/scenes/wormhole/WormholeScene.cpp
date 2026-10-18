// Scene 12 — wormhole transit: approaching a spherical wormhole mouth that shows another universe (a spiral
// galaxy) while lensing our own sky, swirling gas around it; then the plunge down a streaking tunnel and the exit
// into the other sky — leading straight into the galaxy scene.
// Timeline params: lens (bending strength), gas, scale (render scale).

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Camera.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace demo {

namespace {

constexpr float kRadius = 10.0f;
constexpr double kApproachEnd = 0.72;  // fraction of the entry spent approaching the mouth

class WormholeScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        m_shader = &resources.shaders().loadFullscreen("scene/wormhole", "scenes/wormhole/wormhole.frag");
        resources.noiseVolume();
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        if (!m_shader->valid()) return;
        const SceneParams& params = context.params;
        const double t = context.localTime;
        const double progress = t / context.duration;

        // Approach the mouth, then the tunnel, then out into the other sky.
        const double approach = motion::smoothstep(0.0, 1.0, std::min(progress / kApproachEnd, 1.0));
        const double distance = 220.0 + (kRadius * 1.05 - 220.0) * approach;
        const float tunnel = static_cast<float>(motion::smoothstep(0.64, 0.74, progress) * (1.0 - motion::smoothstep(0.9, 0.97, progress)));
        const float exit = static_cast<float>(motion::smoothstep(0.9, 0.97, progress));
        const double tunnelTime = std::max(0.0, t - 0.64 * context.duration);
        const float travel = static_cast<float>(tunnelTime * tunnelTime * 0.8);
        const glm::vec3 position(static_cast<float>(std::sin(0.17 * t) * distance * 0.12),
                                 static_cast<float>(distance * 0.08 + 2.0), static_cast<float>(distance));
        const Camera camera = Camera::lookAt(position, glm::vec3(0.0f), m_focal, static_cast<float>(tunnel * tunnelTime * 0.4));

        const float scale = std::clamp(params.get("scale", m_scale), 0.25f, 1.0f);
        RenderTarget* output = &target;
        if (scale < 0.99f) {
            m_lowRes.resize(glm::max(glm::ivec2(glm::vec2(context.size) * scale), glm::ivec2(1)));
            m_lowRes.bind();
            output = &m_lowRes;
        }

        m_resources->bindNoiseVolume(0);
        m_shader->use();
        camera.apply(*m_shader);
        m_shader->set("uResolution", glm::vec2(output->size()));
        m_shader->set("uTime", static_cast<float>(t));
        m_shader->set("uNoise", 0);
        m_shader->set("uRadius", kRadius);
        m_shader->set("uLens", params.get("lens", m_lens));
        m_shader->set("uGas", params.get("gas", m_gas));
        m_shader->set("uTunnel", tunnel);
        m_shader->set("uExit", exit);
        m_shader->set("uTravel", travel);
        m_shader->set("uPulse", context.sync.beatPulse(5.0f) * 0.6f);
        m_shader->set("uGalaxyDir", glm::normalize(glm::vec3(0.15f, 0.1f, -1.0f)));
        m_shader->set("uRemnantDir", glm::normalize(glm::vec3(-0.6f, 0.3f, 0.7f)));
        m_resources->drawFullscreen();

        if (output != &target) {
            const glm::ivec2 from = m_lowRes.size(), to = target.size();
            glBlitNamedFramebuffer(m_lowRes.fbo(), target.fbo(), 0, 0, from.x, from.y, 0, 0, to.x, to.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            target.bind();
        }
    }

    void imgui() override {
        ImGui::SliderFloat("lensing", &m_lens, 0.0f, 2.0f);
        ImGui::SliderFloat("gas", &m_gas, 0.0f, 4.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
        ImGui::SliderFloat("render scale", &m_scale, 0.25f, 1.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_shader = nullptr;
    RenderTarget m_lowRes{GL_RGBA16F};
    float m_lens = 1.0f;
    float m_gas = 1.0f;
    float m_focal = 1.2f;
    float m_scale = 1.0f;
};

}  // namespace

REGISTER_SCENE("wormhole", WormholeScene);

}  // namespace demo

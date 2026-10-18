// Scene 4 — 3D warp starfield: cruising through a star cloud, accelerating into hyperspace streaks with a
// rolling camera and a tunnel glow. Pair with a "flash" transition into the next scene for the exit.
// Timeline params: speed = cruise speed multiplier (default 1), hyper = top speed / cruise speed (default 40),
//                  count = number of stars (default 60000), kick = kick instrument number (default 1).

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace demo {

namespace {

class WarpScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_tunnel = &shaders.loadFullscreen("scene/warp/tunnel", "scenes/warp/tunnel.frag");
        m_stars = &shaders.load("scene/warp/stars", {{GL_VERTEX_SHADER, "scenes/warp/stars.vert"},
                                                     {GL_FRAGMENT_SHADER, "scenes/warp/stars.frag"}});
    }

    void render(const SceneContext& context, RenderTarget& /*target*/) override {
        const double duration = context.duration;
        const double t = context.localTime;
        const double cruise = 5.0 * m_speed * context.params.get("speed", 1.0f);
        const double top = cruise * context.params.get("hyper", 40.0f);

        auto speedAt = [&](double s) { return cruise + (top - cruise) * std::pow(motion::smoothstep(0.3, 0.97, s / duration), 3.0); };
        auto hyperAt = [&](double s) { return motion::smoothstep(0.55, 1.0, s / duration); };
        auto rollRateAt = [&](double s) { return 0.06 + 1.2 * std::pow(hyperAt(s), 2.0); };

        const float travel = static_cast<float>(motion::integrate(speedAt, t));
        const float roll = static_cast<float>(motion::integrate(rollRateAt, t));
        const float hyper = static_cast<float>(hyperAt(t));
        const glm::vec2 resolution(context.size);

        if (m_tunnel->valid()) {
            m_tunnel->use();
            m_tunnel->set("uResolution", resolution);
            m_tunnel->set("uTravel", travel);
            m_tunnel->set("uHyper", hyper);
            m_tunnel->set("uRoll", roll);
            m_resources->drawFullscreen();
        }

        if (m_stars->valid()) {
            const int count = std::clamp(static_cast<int>(context.params.get("count", static_cast<float>(m_count))), 1000, 1000000);
            m_stars->use();
            m_stars->set("uResolution", resolution);
            m_stars->set("uTravel", travel);
            m_stars->set("uSpeed", static_cast<float>(speedAt(t)));
            m_stars->set("uShutter", m_shutter);
            m_stars->set("uDepth", m_depth);
            m_stars->set("uRoll", roll);
            m_stars->set("uFocal", m_focal);
            m_stars->set("uHyper", hyper);
            m_stars->set("uKick", context.sync.instrumentPulse(static_cast<int>(context.params.get("kick", 1.0f)), 7.0f));

            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            m_resources->drawProcedural(GL_TRIANGLES, 0, count * 6);
            glDisable(GL_BLEND);
        }
    }

    void imgui() override {
        ImGui::SliderInt("stars", &m_count, 1000, 500000);
        ImGui::SliderFloat("cruise speed", &m_speed, 0.0f, 5.0f);
        ImGui::SliderFloat("shutter", &m_shutter, 0.0f, 0.1f, "%.3f s");
        ImGui::SliderFloat("depth", &m_depth, 10.0f, 200.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.3f, 3.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_tunnel = nullptr;
    ShaderProgram* m_stars = nullptr;
    int m_count = 60000;
    float m_speed = 1.0f;
    float m_shutter = 1.0f / 60.0f;
    float m_depth = 60.0f;
    float m_focal = 1.0f;
};

}  // namespace

REGISTER_SCENE("warp", WarpScene);

}  // namespace demo

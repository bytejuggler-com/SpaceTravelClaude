// Scene 2 — classic horizontal parallax starfield with a gas giant drifting past.
// Stars accelerate over the last quarter of the entry, handing over to the warp scene.
// Timeline params: speed (default 1), boost = end-of-scene speed multiplier (default 8),
//                  density = stars multiplier (default 1), planet = 0 to hide the planet (default 1).

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace demo {

namespace {

constexpr int kLayers = 6;
constexpr int kFarLayers = 3;  // drawn behind the planet; the rest in front

class HStarsScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_background = &shaders.loadFullscreen("scene/hstars/background", "scenes/hstars/background.frag");
        m_planet = &shaders.loadFullscreen("scene/hstars/planet", "scenes/hstars/planet.frag");
        m_stars = &shaders.load("scene/hstars/stars", {{GL_VERTEX_SHADER, "scenes/hstars/stars.vert"},
                                                       {GL_FRAGMENT_SHADER, "scenes/hstars/stars.frag"}});
    }

    void render(const SceneContext& context, RenderTarget& /*target*/) override {
        const double baseSpeed = 0.12 * m_speed * context.params.get("speed", 1.0f);  // screen widths / s
        const double boost = context.params.get("boost", 8.0f);
        const double duration = context.duration;
        auto speedAt = [&](double t) {
            return baseSpeed * (1.0 + boost * std::pow(motion::smoothstep(0.75, 1.0, t / duration), 2.0));
        };
        const float travel = static_cast<float>(motion::integrate(speedAt, context.localTime));
        const float speed = static_cast<float>(speedAt(context.localTime));
        const int perLayer = std::max(1, static_cast<int>(m_starsPerLayer * context.params.get("density", 1.0f)));
        const glm::vec2 resolution(context.size);

        if (m_background->valid()) {
            m_background->use();
            m_background->set("uResolution", resolution);
            m_background->set("uTravel", travel);
            m_resources->drawFullscreen();
        }

        const bool starsOk = m_stars->valid();
        if (starsOk) {
            m_stars->set("uResolution", resolution);
            m_stars->set("uTravel", travel);
            m_stars->set("uSpeed", speed);
            m_stars->set("uStarsPerLayer", perLayer);
            m_stars->set("uLayers", kLayers);
            m_stars->set("uTime", static_cast<float>(context.localTime));
            m_stars->set("uBeatPulse", context.sync.beatPulse(6.0f));
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        if (starsOk) {
            m_stars->use();
            m_resources->drawProcedural(GL_TRIANGLES, 0, kFarLayers * perLayer * 6);
        }

        if (m_planet->valid() && context.params.get("planet", 1.0f) > 0.0f) {
            const float aspect = resolution.x / resolution.y;
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            m_planet->use();
            m_planet->set("uResolution", resolution);
            // uCenter is in screen-height units, so one screen width = aspect. Starts centred on the right edge and
            // drifts half a screen width per unit of travel: a parallax between the far layers (<= 0.3) drawn behind
            // it and the near ones (up to 1) drawn in front.
            m_planet->set("uCenter", glm::vec2(aspect * 0.5f - travel * 0.5f * aspect, -0.38f));
            m_planet->set("uRadius", m_planetRadius);
            m_planet->set("uRotation", static_cast<float>(context.localTime * 0.06));
            m_resources->drawFullscreen();
            glBlendFunc(GL_ONE, GL_ONE);
        }

        if (starsOk) {
            m_stars->use();
            m_resources->drawProcedural(GL_TRIANGLES, kFarLayers * perLayer * 6, (kLayers - kFarLayers) * perLayer * 6);
        }
        glDisable(GL_BLEND);
    }

    void imgui() override {
        ImGui::SliderFloat("speed", &m_speed, 0.0f, 5.0f);
        ImGui::SliderInt("stars per layer", &m_starsPerLayer, 100, 10000);
        ImGui::SliderFloat("planet radius", &m_planetRadius, 0.1f, 1.5f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_background = nullptr;
    ShaderProgram* m_planet = nullptr;
    ShaderProgram* m_stars = nullptr;
    float m_speed = 1.0f;
    int m_starsPerLayer = 1200;
    float m_planetRadius = 0.5f;
};

}  // namespace

REGISTER_SCENE("hstars", HStarsScene);

}  // namespace demo

// Scene 5 — patrolling a giant star: boiling surface, corona flames, prominence arcs. The camera orbits with a
// slow approach for a close fly-by. The star is raymarched at reduced resolution (render scale) and composited
// over a full-resolution sky.
// Timeline params: orbit = orbit speed multiplier (default 1), scale = star render scale (default 0.5),
//                  snare = instrument that triggers corona flares (default 2).

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Camera.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace demo {

namespace {

class CoronaScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_star = &shaders.loadFullscreen("scene/corona/star", "scenes/corona/star.frag");
        m_composite = &shaders.loadFullscreen("scene/corona/composite", "scenes/corona/composite.frag");
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const double t = context.localTime;
        const Camera camera = cameraAt(t * m_orbit * context.params.get("orbit", 1.0f));
        const float scale = std::clamp(context.params.get("scale", m_renderScale), 0.2f, 1.0f);
        const glm::ivec2 starSize = glm::max(glm::ivec2(glm::vec2(context.size) * scale), glm::ivec2(1));

        m_starLayer.resize(starSize);
        m_starLayer.bind();
        m_starLayer.clear(glm::vec4(0.0f));
        if (m_star->valid()) {
            const int snare = static_cast<int>(context.params.get("snare", 2.0f));
            m_star->use();
            camera.apply(*m_star);
            m_star->set("uResolution", glm::vec2(starSize));
            m_star->set("uTime", static_cast<float>(t));
            m_star->set("uRotation", static_cast<float>(t * 0.03));
            m_star->set("uSurfaceIntensity", m_surfaceIntensity);
            m_star->set("uCoronaIntensity", m_coronaIntensity);
            m_star->set("uCoronaReach", m_coronaReach);
            m_star->set("uProminences", m_prominences);
            m_star->set("uFlare", context.sync.instrumentPulse(snare, 4.0f) * m_flare);
            m_star->set("uPulse", context.sync.beatPulse(6.0f));
            m_resources->drawFullscreen();
        }

        target.bind();
        if (m_composite->valid()) {
            m_starLayer.bindTexture(0);
            m_composite->use();
            camera.apply(*m_composite);
            m_composite->set("uResolution", glm::vec2(context.size));
            m_composite->set("uStar", 0);
            m_resources->drawFullscreen();
        }
    }

    void imgui() override {
        ImGui::SliderFloat("render scale", &m_renderScale, 0.2f, 1.0f, "%.2f");
        ImGui::SliderFloat("orbit speed", &m_orbit, 0.0f, 4.0f);
        ImGui::SliderFloat("surface", &m_surfaceIntensity, 0.0f, 20.0f);
        ImGui::SliderFloat("corona", &m_coronaIntensity, 0.0f, 6.0f);
        ImGui::SliderFloat("corona reach", &m_coronaReach, 0.05f, 2.0f);
        ImGui::SliderFloat("prominences", &m_prominences, 0.0f, 4.0f);
        ImGui::SliderFloat("snare flare", &m_flare, 0.0f, 3.0f);
    }

private:
    // Orbit around the star with a periodic approach; the camera aims a little past the star so it drifts
    // across the frame like a patrol fly-by.
    static Camera cameraAt(double t) {
        const double angle = 0.4 + 0.11 * t;
        const double elevation = 0.28 * std::sin(0.17 * t + 0.8);
        const double approach = 0.5 - 0.5 * std::cos(t * 0.33);  // 0 = far, 1 = closest
        const double distance = 3.6 + (2.35 - 3.6) * approach;
        const glm::vec3 position(static_cast<float>(distance * std::cos(elevation) * std::cos(angle)),
                                 static_cast<float>(distance * std::sin(elevation)),
                                 static_cast<float>(distance * std::cos(elevation) * std::sin(angle)));
        const glm::vec3 toStar = glm::normalize(-position);
        const glm::vec3 side = glm::normalize(glm::cross(toStar, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 aim = side * static_cast<float>(0.55 * std::sin(0.21 * t + 0.3)) +
                              glm::vec3(0.0f, static_cast<float>(0.15 * std::sin(0.13 * t)), 0.0f);
        return Camera::lookAt(position, aim, 1.1f, static_cast<float>(0.08 * std::sin(0.19 * t)));
    }

    Resources* m_resources = nullptr;
    ShaderProgram* m_star = nullptr;
    ShaderProgram* m_composite = nullptr;
    RenderTarget m_starLayer{GL_RGBA16F};
    float m_renderScale = 0.5f;
    float m_orbit = 1.0f;
    float m_surfaceIntensity = 5.0f;
    float m_coronaIntensity = 1.5f;
    float m_coronaReach = 0.6f;
    float m_prominences = 1.0f;
    float m_flare = 0.8f;
};

}  // namespace

REGISTER_SCENE("corona", CoronaScene);

}  // namespace demo

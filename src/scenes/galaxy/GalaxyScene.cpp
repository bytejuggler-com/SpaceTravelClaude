// Scene 13 — spiral galaxy: ~800k stars, glowing gas and dark dust lanes on density-wave orbits (shaders in
// shaders/scenes/galaxy/), over a core glow and disc haze. The camera orbits at an angle, then dives into the
// disc among the stars, looking toward the core — leading into the black hole that follows.
// Timeline params: stars (count), speed (orbit speed), twist, gas, dust, core (glow), brightness.

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Camera.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace demo {

namespace {

class GalaxyScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_background = &shaders.loadFullscreen("scene/galaxy/background", "scenes/galaxy/background.frag");
        m_stars = &shaders.load("scene/galaxy/stars", {{GL_VERTEX_SHADER, "scenes/galaxy/stars.vert"},
                                                       {GL_FRAGMENT_SHADER, "scenes/galaxy/stars.frag"}});
        m_clouds = &shaders.load("scene/galaxy/clouds", {{GL_VERTEX_SHADER, "scenes/galaxy/clouds.vert"},
                                                         {GL_FRAGMENT_SHADER, "scenes/galaxy/clouds.frag"}});
    }

    void render(const SceneContext& context, RenderTarget& /*target*/) override {
        const SceneParams& params = context.params;
        const double t = context.localTime;
        const double progress = t / context.duration;

        // Orbit at an angle while closing in, then dive into the disc and look across it toward the core.
        const double approach = motion::smoothstep(0.0, 0.65, progress);
        const double dive = motion::smoothstep(0.62, 1.0, progress);
        const double distance = glm::mix(glm::mix(260.0, 150.0, approach), 32.0, dive);
        const double elevation = glm::radians(glm::mix(glm::mix(55.0, 32.0, approach), 6.0, dive));
        const double azimuth = 0.6 + 0.12 * t;
        const glm::vec3 position(static_cast<float>(distance * std::cos(elevation) * std::sin(azimuth)),
                                 static_cast<float>(distance * std::sin(elevation)),
                                 static_cast<float>(distance * std::cos(elevation) * std::cos(azimuth)));
        const Camera camera = Camera::lookAt(position, glm::vec3(0.0f), m_focal);

        const glm::vec2 resolution(context.size);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        const glm::mat4 projection =
            glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, 0.1f, 2000.0f);
        const glm::mat4 viewProj = projection * view;

        const float time = static_cast<float>(t);
        const float orbitSpeed = m_orbitSpeed * params.get("speed", 1.0f);
        const float twist = params.get("twist", m_twist);
        const float pattern = time * 0.02f;
        auto setModel = [&](ShaderProgram& program) {
            program.set("uViewProj", viewProj);
            program.set("uCamPos", camera.position);
            program.set("uTime", time);
            program.set("uOrbitSpeed", orbitSpeed);
            program.set("uPattern", pattern);
            program.set("uTwist", twist);
        };

        if (m_background->valid()) {
            m_background->use();
            camera.apply(*m_background);
            m_background->set("uResolution", resolution);
            m_background->set("uCoreGlow", params.get("core", m_coreGlow) * (1.0f + 0.25f * context.sync.beatPulse(4.0f)));
            m_background->set("uHaze", m_haze);
            m_resources->drawFullscreen();
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        if (m_stars->valid()) {
            glEnable(GL_PROGRAM_POINT_SIZE);
            m_stars->use();
            setModel(*m_stars);
            m_stars->set("uPointScale", 220.0f * resolution.y / 1080.0f);
            m_stars->set("uBrightness", params.get("brightness", m_brightness));
            m_stars->set("uBulgeFraction", 0.18f);
            const int count = std::clamp(static_cast<int>(params.get("stars", static_cast<float>(m_starCount))), 10000, 4000000);
            m_resources->drawProcedural(GL_POINTS, 0, count);
            glDisable(GL_PROGRAM_POINT_SIZE);
        }
        if (m_clouds->valid()) {
            m_clouds->use();
            setModel(*m_clouds);
            m_clouds->set("uCamRight", camera.right);
            m_clouds->set("uCamUp", camera.up);

            m_clouds->set("uMode", 0);
            m_clouds->set("uStrength", params.get("gas", m_gas));
            m_resources->drawProcedural(GL_TRIANGLES, 0, m_gasCount * 6);

            glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);  // dust absorbs what is already there
            m_clouds->set("uMode", 1);
            m_clouds->set("uStrength", params.get("dust", m_dust));
            m_resources->drawProcedural(GL_TRIANGLES, 0, m_dustCount * 6);
        }
        glDisable(GL_BLEND);
    }

    void imgui() override {
        ImGui::SliderInt("stars", &m_starCount, 10000, 3000000);
        ImGui::SliderFloat("brightness", &m_brightness, 0.0f, 0.2f, "%.3f");
        ImGui::SliderFloat("orbit speed", &m_orbitSpeed, 0.0f, 5.0f);
        ImGui::SliderFloat("twist", &m_twist, 0.0f, 0.12f, "%.3f");
        ImGui::SliderInt("gas clouds", &m_gasCount, 0, 30000);
        ImGui::SliderFloat("gas", &m_gas, 0.0f, 4.0f);
        ImGui::SliderInt("dust clouds", &m_dustCount, 0, 60000);
        ImGui::SliderFloat("dust", &m_dust, 0.0f, 4.0f);
        ImGui::SliderFloat("core glow", &m_coreGlow, 0.0f, 4.0f);
        ImGui::SliderFloat("disc haze", &m_haze, 0.0f, 1.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_background = nullptr;
    ShaderProgram* m_stars = nullptr;
    ShaderProgram* m_clouds = nullptr;

    int m_starCount = 800000;
    float m_brightness = 0.09f;
    float m_orbitSpeed = 1.0f;
    float m_twist = 0.045f;
    int m_gasCount = 6000;
    float m_gas = 0.6f;
    int m_dustCount = 14000;
    float m_dust = 0.25f;
    float m_coreGlow = 1.0f;
    float m_haze = 0.25f;
    float m_focal = 1.3f;
};

}  // namespace

REGISTER_SCENE("galaxy", GalaxyScene);

}  // namespace demo

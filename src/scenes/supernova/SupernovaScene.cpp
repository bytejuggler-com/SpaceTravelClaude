// Scene 11 — supernova: a pulsating red supergiant grows unstable and collapses; on the beat a third of the way
// in, a blinding flash; then a turbulent shock shell races outward with hot fronts and cooling filaments, an
// equatorial ring flares, debris streaks past and a pulsar starts sweeping its beams. The camera shakes at the
// flash and pulls back as the shell grows.
// Volume at reduced resolution (shaders/scenes/supernova/volume.frag), stars + flash at full resolution, debris
// streaks added on top (debris.vert, sharing the warp's streak fragment shader).
// Timeline params: scale (volume resolution), steps, shell (brightness), debris (count).

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

constexpr double kFlashFraction = 1.0 / 3.0;  // 16 beats into a 48-beat entry
constexpr double kCollapseSeconds = 1.2;
constexpr float kStarRadius = 22.0f;

double shellRadiusAt(double age) { return age <= 0.0 ? 0.0 : 150.0 * (1.0 - std::exp(-age * 0.25)) + age * 1.5; }

class SupernovaScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_volume = &shaders.loadFullscreen("scene/supernova/volume", "scenes/supernova/volume.frag");
        m_composite = &shaders.loadFullscreen("scene/supernova/composite", "scenes/supernova/composite.frag");
        m_debris = &shaders.load("scene/supernova/debris", {{GL_VERTEX_SHADER, "scenes/supernova/debris.vert"},
                                                            {GL_FRAGMENT_SHADER, "scenes/warp/stars.frag"}});
        resources.noiseVolume();
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const SceneParams& params = context.params;
        const double t = context.localTime;
        const double flashTime = context.duration * kFlashFraction;
        const double age = t - flashTime;  // seconds since the flash (negative before)

        // Star: pulsating, increasingly unstable, then collapsing in the last moment before the flash.
        const double instability = std::pow(std::clamp(t / flashTime, 0.0, 1.0), 3.0);
        double starRadius = kStarRadius * (1.0 + 0.04 * std::sin(t * 1.5) * (1.0 + 2.0 * instability));
        starRadius *= 1.0 - motion::smoothstep(flashTime - kCollapseSeconds, flashTime, t);
        if (age >= 0.0) starRadius = 0.0;

        const double shellRadius = shellRadiusAt(age);
        const float flash = age >= 0.0 ? static_cast<float>(12.0 * std::exp(-age * 2.5)) : 0.0f;
        const float ringIntensity = age > 0.0 ? static_cast<float>(motion::smoothstep(0.5, 1.5, age) * std::exp(-age * 0.15) * 3.0) : 0.0f;
        const float pulsar = static_cast<float>(motion::smoothstep(3.0, 6.0, age));

        // Camera: slow orbit; shake at the flash; pull back as the shell grows.
        const double distance = 75.0 + 185.0 * motion::smoothstep(0.0, 12.0, age);
        const double azimuth = 0.4 + 0.05 * t;
        const float shake = age > 0.0 ? static_cast<float>(std::exp(-age * 2.0) * 2.5) : 0.0f;
        const glm::vec3 jolt(std::sin(t * 61.0) * shake, std::sin(t * 47.0 + 1.0) * shake, std::sin(t * 53.0 + 2.0) * shake);
        const glm::vec3 position = glm::vec3(static_cast<float>(distance * std::sin(azimuth)), static_cast<float>(distance * 0.22),
                                             static_cast<float>(distance * std::cos(azimuth))) + jolt;
        const Camera camera = Camera::lookAt(position, glm::vec3(0.0f), m_focal);

        const float scale = std::clamp(params.get("scale", m_scale), 0.2f, 1.0f);
        m_volumeTarget.resize(glm::max(glm::ivec2(glm::vec2(context.size) * scale), glm::ivec2(1)));
        m_volumeTarget.bind();
        m_volumeTarget.clear(glm::vec4(0.0f));
        if (m_volume->valid()) {
            m_resources->bindNoiseVolume(0);
            m_volume->use();
            camera.apply(*m_volume);
            m_volume->set("uResolution", glm::vec2(m_volumeTarget.size()));
            m_volume->set("uTime", static_cast<float>(t));
            m_volume->set("uNoise", 0);
            m_volume->set("uSteps", static_cast<int>(params.get("steps", static_cast<float>(m_steps))));
            m_volume->set("uStarRadius", static_cast<float>(starRadius));
            m_volume->set("uInstability", static_cast<float>(instability));
            m_volume->set("uShellRadius", static_cast<float>(shellRadius));
            m_volume->set("uShellAge", static_cast<float>(std::max(age, 0.0)));
            m_volume->set("uShellBrightness", params.get("shell", m_shellBrightness) *
                                                  static_cast<float>(0.35 + 2.5 * std::exp(-std::max(age, 0.0) * 0.4)) *
                                                  (1.0f + 0.2f * context.sync.beatPulse(5.0f)));
            m_volume->set("uRingRadius", static_cast<float>(40.0 + std::max(age, 0.0) * 14.0));
            m_volume->set("uRingIntensity", ringIntensity);
            m_volume->set("uPulsar", pulsar);
            m_volume->set("uPulsarAngle", static_cast<float>(t * 7.0));
            m_resources->drawFullscreen();
        }

        target.bind();
        if (m_composite->valid()) {
            m_volumeTarget.bindTexture(0);
            m_composite->use();
            camera.apply(*m_composite);
            m_composite->set("uResolution", glm::vec2(context.size));
            m_composite->set("uVolume", 0);
            m_composite->set("uFlash", flash);
            m_resources->drawFullscreen();
        }

        if (m_debris->valid() && age > 0.0) {
            const glm::vec2 resolution(context.size);
            const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
            const glm::mat4 projection = glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, 0.5f, 5000.0f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            m_debris->use();
            m_debris->set("uViewProj", projection * view);
            m_debris->set("uResolution", resolution);
            m_debris->set("uRadius", static_cast<float>(shellRadius));
            m_debris->set("uPreviousRadius", static_cast<float>(shellRadiusAt(age - 0.12)));
            m_debris->set("uBrightness", static_cast<float>(2.5 * std::exp(-age * 0.25)));
            const int count = std::clamp(static_cast<int>(params.get("debris", static_cast<float>(m_debrisCount))), 0, 50000);
            m_resources->drawProcedural(GL_TRIANGLES, 0, count * 6);
            glDisable(GL_BLEND);
        }
    }

    void imgui() override {
        ImGui::SliderFloat("volume scale", &m_scale, 0.2f, 1.0f);
        ImGui::SliderInt("steps", &m_steps, 16, 256);
        ImGui::SliderFloat("shell brightness", &m_shellBrightness, 0.0f, 5.0f);
        ImGui::SliderInt("debris", &m_debrisCount, 0, 20000);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_volume = nullptr;
    ShaderProgram* m_composite = nullptr;
    ShaderProgram* m_debris = nullptr;
    RenderTarget m_volumeTarget{GL_RGBA16F};
    float m_scale = 0.5f;
    int m_steps = 80;
    float m_shellBrightness = 1.0f;
    int m_debrisCount = 4000;
    float m_focal = 1.1f;
};

}  // namespace

REGISTER_SCENE("supernova", SupernovaScene);

}  // namespace demo

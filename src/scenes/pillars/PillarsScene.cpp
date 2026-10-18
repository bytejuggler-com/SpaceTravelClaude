// Scene 3 — nebula crossing: flying among towering dust pillars toward the star cluster that sculpts them.
// Volume raymarched at reduced resolution (shaders/scenes/pillars/volume.frag), stars and cluster glow composited
// at full resolution. The camera rises along the pillars and ends over their heads, looking into the light.
// Timeline params: scale (volume resolution), steps, lightsteps, dust, gas, scatter, rim.

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Camera.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace demo {

namespace {

// Catmull-Rom through four points, t in [0, 1] across the middle three segments' span.
glm::vec3 catmullRom(const std::array<glm::vec3, 4>& p, float t) {
    const float segments = 3.0f;
    const float x = std::clamp(t, 0.0f, 1.0f) * segments;
    const int i = std::min(static_cast<int>(x), 2);
    const float u = x - static_cast<float>(i);
    const glm::vec3 p0 = p[std::max(i - 1, 0)], p1 = p[i], p2 = p[i + 1], p3 = p[std::min(i + 2, 3)];
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u * u * u);
}

class PillarsScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_volume = &shaders.loadFullscreen("scene/pillars/volume", "scenes/pillars/volume.frag");
        m_composite = &shaders.loadFullscreen("scene/pillars/composite", "scenes/pillars/composite.frag");
        resources.noiseVolume();
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const SceneParams& params = context.params;
        const float progress = static_cast<float>(motion::smoothstep(0.0, 1.0, context.localTime / context.duration));

        // Rise between the pillars, ending over their heads looking toward the cluster.
        const std::array<glm::vec3, 4> path = {glm::vec3(-20.0f, -30.0f, 260.0f), glm::vec3(5.0f, -10.0f, 160.0f),
                                               glm::vec3(20.0f, 45.0f, 95.0f), glm::vec3(10.0f, 95.0f, 40.0f)};
        const std::array<glm::vec3, 4> aims = {glm::vec3(0.0f, -20.0f, 0.0f), glm::vec3(0.0f, 10.0f, -20.0f),
                                               glm::vec3(5.0f, 60.0f, -80.0f), glm::vec3(10.0f, 120.0f, -150.0f)};
        const Camera camera = Camera::lookAt(catmullRom(path, progress), catmullRom(aims, progress), m_focal);

        const glm::vec3 lightPos(10.0f, 200.0f, -380.0f);
        const glm::vec3 lightColor = glm::vec3(0.85f, 0.92f, 1.0f) * m_lightIntensity *
                                     (1.0f + 0.15f * context.sync.beatPulse(4.0f));

        const float scale = std::clamp(params.get("scale", m_scale), 0.2f, 1.0f);
        m_volumeTarget.resize(glm::max(glm::ivec2(glm::vec2(context.size) * scale), glm::ivec2(1)));
        m_volumeTarget.bind();
        m_volumeTarget.clear(glm::vec4(0.0f));
        if (m_volume->valid()) {
            m_resources->bindNoiseVolume(0);
            m_volume->use();
            camera.apply(*m_volume);
            m_volume->set("uResolution", glm::vec2(m_volumeTarget.size()));
            m_volume->set("uTime", static_cast<float>(context.localTime));
            m_volume->set("uNoise", 0);
            m_volume->set("uLightPos", lightPos);
            m_volume->set("uLightColor", lightColor);
            m_volume->set("uSteps", static_cast<int>(params.get("steps", static_cast<float>(m_steps))));
            m_volume->set("uLightSteps", static_cast<int>(params.get("lightsteps", static_cast<float>(m_lightSteps))));
            m_volume->set("uDust", params.get("dust", m_dust));
            m_volume->set("uGas", params.get("gas", m_gas));
            m_volume->set("uScatter", params.get("scatter", m_scatter));
            m_volume->set("uRim", params.get("rim", m_rim));
            m_volume->set("uFar", 520.0f);
            m_resources->drawFullscreen();
        }

        target.bind();
        if (m_composite->valid()) {
            m_volumeTarget.bindTexture(0);
            m_composite->use();
            camera.apply(*m_composite);
            m_composite->set("uResolution", glm::vec2(context.size));
            m_composite->set("uVolume", 0);
            m_composite->set("uLightPos", lightPos);
            m_composite->set("uLightColor", lightColor);
            m_resources->drawFullscreen();
        }
    }

    void imgui() override {
        ImGui::SliderFloat("volume scale", &m_scale, 0.2f, 1.0f);
        ImGui::SliderInt("steps", &m_steps, 16, 256);
        ImGui::SliderInt("light steps", &m_lightSteps, 1, 16);
        ImGui::SliderFloat("dust", &m_dust, 0.0f, 4.0f);
        ImGui::SliderFloat("gas", &m_gas, 0.0f, 4.0f);
        ImGui::SliderFloat("scatter", &m_scatter, 0.0f, 2.0f);
        ImGui::SliderFloat("rim", &m_rim, 0.0f, 4.0f);
        ImGui::SliderFloat("light", &m_lightIntensity, 0.0f, 10.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_volume = nullptr;
    ShaderProgram* m_composite = nullptr;
    RenderTarget m_volumeTarget{GL_RGBA16F};
    float m_scale = 0.5f;
    int m_steps = 96;
    int m_lightSteps = 6;
    float m_dust = 1.0f;
    float m_gas = 1.0f;
    float m_scatter = 0.12f;
    float m_rim = 1.0f;
    float m_lightIntensity = 2.5f;
    float m_focal = 1.1f;
};

}  // namespace

REGISTER_SCENE("pillars", PillarsScene);

}  // namespace demo

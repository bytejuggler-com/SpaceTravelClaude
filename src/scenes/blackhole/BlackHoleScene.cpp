// Scene 14 — black hole: bent light paths produce the shadow, photon ring, the disk arching over the shadow and
// a lensed nebula band behind it. The camera stays centered on the hole, slightly above the disk plane,
// drifting slowly and closing in over the entry.
// Timeline params (all optional; defaults are the overlay slider values):
//   incl = camera height above the disk plane in degrees, zoom = distance divider (2 = twice as close),
//   inner / outer = disk radii (horizon = 1), temp, brightness, doppler (0 = symmetric, 1 = relativistic),
//   lensing (0..1), jets, quality (ray step density), scale (render scale), snare = hot-spot trigger instrument.

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Camera.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace demo {

namespace {

class BlackHoleScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        m_shader = &resources.shaders().loadFullscreen("scene/blackhole", "scenes/blackhole/blackhole.frag");
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        if (!m_shader->valid()) return;
        const SceneParams& params = context.params;
        const double t = context.localTime;
        const float quality = std::clamp(params.get("quality", m_quality), 0.25f, 4.0f);
        const float scale = std::clamp(params.get("scale", m_renderScale), 0.25f, 1.0f);

        // Slow drift, centered on the hole, closing in over the entry.
        const double inclination = (params.get("incl", m_inclination) + 2.0 * std::sin(0.2 * t)) * std::numbers::pi / 180.0;
        const double azimuth = -0.4 + 0.035 * t;
        const double distance = m_distance / params.get("zoom", 1.0f) * (1.0 - 0.3 * motion::smoothstep(0.0, 1.0, t / context.duration));
        const glm::vec3 position(static_cast<float>(distance * std::cos(inclination) * std::sin(azimuth)),
                                 static_cast<float>(distance * std::sin(inclination)),
                                 static_cast<float>(distance * std::cos(inclination) * std::cos(azimuth)));
        const Camera camera = Camera::lookAt(position, glm::vec3(0.0f), m_focal);

        // Render straight into the scene target, or into a smaller one and upscale.
        RenderTarget* output = &target;
        if (scale < 0.99f) {
            m_lowRes.resize(glm::max(glm::ivec2(glm::vec2(context.size) * scale), glm::ivec2(1)));
            m_lowRes.bind();
            output = &m_lowRes;
        }

        m_shader->use();
        camera.apply(*m_shader);
        m_shader->set("uResolution", glm::vec2(output->size()));
        m_shader->set("uTime", static_cast<float>(t));
        m_shader->set("uMaxSteps", static_cast<int>(220.0f * quality));
        m_shader->set("uStepScale", 1.0f / quality);
        m_shader->set("uLensing", params.get("lensing", m_lensing));
        m_shader->set("uInner", params.get("inner", m_inner));
        m_shader->set("uOuter", params.get("outer", m_outer));
        m_shader->set("uTemp", params.get("temp", m_temperature));
        m_shader->set("uBrightness", params.get("brightness", m_brightness));
        m_shader->set("uDoppler", params.get("doppler", m_doppler));
        m_shader->set("uDiskSpeed", m_diskSpeed);
        m_shader->set("uOpacity", m_opacity);
        m_shader->set("uJets", params.get("jets", m_jets));
        m_shader->set("uBackground", m_background);
        m_shader->set("uBandNormal", glm::normalize(glm::vec3(0.5f, 0.8f, 0.3f)));
        m_shader->set("uSpotPulse", context.sync.instrumentPulse(static_cast<int>(params.get("snare", 2.0f)), 5.0f));
        m_resources->drawFullscreen();

        if (output != &target) {
            const glm::ivec2 from = m_lowRes.size();
            const glm::ivec2 to = target.size();
            glBlitNamedFramebuffer(m_lowRes.fbo(), target.fbo(), 0, 0, from.x, from.y, 0, 0, to.x, to.y,
                                   GL_COLOR_BUFFER_BIT, GL_LINEAR);
            target.bind();
        }
    }

    void imgui() override {
        ImGui::SliderFloat("inclination", &m_inclination, -30.0f, 60.0f, "%.1f deg");
        ImGui::SliderFloat("distance", &m_distance, 6.0f, 60.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 4.0f);
        ImGui::SliderFloat("lensing", &m_lensing, 0.0f, 1.0f);
        ImGui::SliderFloat("doppler", &m_doppler, 0.0f, 1.0f);
        ImGui::SliderFloat("disk inner", &m_inner, 1.5f, 8.0f);
        ImGui::SliderFloat("disk outer", &m_outer, 4.0f, 30.0f);
        ImGui::SliderFloat("temperature", &m_temperature, 0.2f, 2.5f);
        ImGui::SliderFloat("brightness", &m_brightness, 0.0f, 20.0f);
        ImGui::SliderFloat("opacity", &m_opacity, 0.0f, 1.0f);
        ImGui::SliderFloat("disk speed", &m_diskSpeed, 0.0f, 20.0f);
        ImGui::SliderFloat("jets", &m_jets, 0.0f, 3.0f);
        ImGui::SliderFloat("background", &m_background, 0.0f, 4.0f);
        ImGui::SliderFloat("quality", &m_quality, 0.25f, 4.0f);
        ImGui::SliderFloat("render scale", &m_renderScale, 0.25f, 1.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_shader = nullptr;
    RenderTarget m_lowRes{GL_RGBA16F};
    float m_inclination = 9.0f;
    float m_distance = 26.0f;
    float m_focal = 1.6f;
    float m_lensing = 1.0f;
    float m_doppler = 0.6f;
    float m_inner = 3.0f;
    float m_outer = 13.0f;
    float m_temperature = 1.0f;
    float m_brightness = 5.0f;
    float m_opacity = 0.9f;
    float m_diskSpeed = 6.0f;
    float m_jets = 0.0f;
    float m_background = 1.0f;
    float m_quality = 1.0f;
    float m_renderScale = 1.0f;
};

}  // namespace

REGISTER_SCENE("blackhole", BlackHoleScene);

}  // namespace demo

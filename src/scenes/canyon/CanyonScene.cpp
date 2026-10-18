// Scene 8 — canyon run on an alien moon at dusk: the fighter races low down a winding canyon, banking through
// the turns, in and out of the cliffs' shadows, its own shadow sliding over the rock, dust hanging in the light
// shafts and the ringed giant in the sky.
// Terrain raymarched at reduced resolution with depth (shaders/scenes/canyon/terrain.frag), upscaled with its
// depth, then the fighter mesh drawn at full resolution and depth-tested against it.
// Timeline params: speed, lag (s), scale (terrain resolution), steps, dust, haze.

#include "core/Resources.h"
#include "core/Shader.h"
#include "gfx/Mesh.h"
#include "scene/Camera.h"
#include "scene/SceneRegistry.h"
#include "scenes/asteroids/Fighter.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace demo {

namespace {

constexpr float kFlightHeight = 9.0f;

// Must match canyonCenter in shaders/scenes/canyon/terrain.glsl.
double canyonCenter(double z) { return 40.0 * std::sin(z * 0.006) + 18.0 * std::sin(z * 0.017 + 1.0); }

glm::vec3 pathAt(double z) {
    return {static_cast<float>(canyonCenter(z)), kFlightHeight + static_cast<float>(1.5 * std::sin(z * 0.021)), static_cast<float>(z)};
}

struct Pose {
    glm::vec3 position, forward, up, side;
};

// On the canyon path at distance z, leaning into turns with the sideways acceleration at `speed`.
Pose poseAt(double z, double speed, float bankGain) {
    const glm::vec3 p = pathAt(z), previous = pathAt(z - 1.0), next = pathAt(z + 1.0);
    const glm::vec3 forward = glm::normalize(next - previous);
    const glm::vec3 acceleration = (next - 2.0f * p + previous) * static_cast<float>(speed * speed);
    const glm::vec3 lateral = acceleration - forward * glm::dot(acceleration, forward);
    const glm::vec3 lean = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) + lateral * bankGain);
    const glm::vec3 side = glm::normalize(glm::cross(lean, forward));  // mesh +x
    return {p, forward, glm::cross(forward, side), side};
}

struct Instance {
    glm::mat4 model;
    glm::vec4 color;
};

class CanyonScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_terrain = &shaders.loadFullscreen("scene/canyon/terrain", "scenes/canyon/terrain.frag");
        m_upscale = &shaders.loadFullscreen("common/upscale", "common/upscale.frag");
        m_fighter = &shaders.load("scene/canyon/fighter", {{GL_VERTEX_SHADER, "common/mesh.vert"},
                                                           {GL_FRAGMENT_SHADER, "scenes/canyon/fighter.frag"}});
        m_ship = Mesh(makeFighter());
        resources.noiseVolume();
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const SceneParams& params = context.params;
        const double t = context.localTime;
        const double speed = m_speed * params.get("speed", 1.0f);
        const float bankGain = 0.05f * m_bank;

        const Pose ship = poseAt(speed * t, speed, bankGain);
        const Pose chase = poseAt(speed * (t - params.get("lag", m_lag)), speed, bankGain * 0.3f);
        Camera camera;
        camera.position = chase.position + chase.up * m_cameraHeight - chase.forward * m_cameraBack;
        camera.forward = glm::normalize(ship.position + ship.forward * 8.0f - camera.position);
        camera.right = glm::normalize(glm::cross(camera.forward, chase.up));
        camera.up = glm::cross(camera.right, camera.forward);
        camera.focal = m_focal;

        const glm::vec2 resolution(context.size);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        const glm::mat4 projection = glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, 0.1f, 3000.0f);
        const glm::mat4 viewProj = projection * view;

        const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.5f, 0.18f, 0.85f));  // low dusk sun ahead-left
        const glm::vec3 sunColor = glm::vec3(1.0f, 0.62f, 0.35f) * m_sunIntensity;
        const glm::mat3 shipBasis(ship.side, ship.up, ship.forward);

        // Terrain at reduced resolution, writing depth.
        const float scale = std::clamp(params.get("scale", m_scale), 0.25f, 1.0f);
        m_low.resize(glm::max(glm::ivec2(resolution * scale), glm::ivec2(1)));
        m_low.bind();
        m_low.clear();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_ALWAYS);
        if (m_terrain->valid()) {
            m_resources->bindNoiseVolume(0);
            m_terrain->use();
            camera.apply(*m_terrain);
            m_terrain->set("uNoise", 0);
            m_terrain->set("uResolution", glm::vec2(m_low.size()));
            m_terrain->set("uTime", static_cast<float>(t));
            m_terrain->set("uViewProj", viewProj);
            m_terrain->set("uLightDir", lightDir);
            m_terrain->set("uSunColor", sunColor);
            m_terrain->set("uSteps", static_cast<int>(params.get("steps", static_cast<float>(m_steps))));
            m_terrain->set("uFar", 2500.0f);
            m_terrain->set("uDust", params.get("dust", m_dust));
            m_terrain->set("uHaze", params.get("haze", m_haze));
            m_terrain->set("uShipPos", ship.position);
            m_terrain->set("uShipBasis", shipBasis);
            m_terrain->set("uPlanetDir", glm::normalize(glm::vec3(0.5f, 0.35f, 0.8f)));
            m_resources->drawFullscreen();
        }

        // Upscale color + depth to full resolution, then the fighter on top.
        m_frame.resize(context.size);
        m_frame.bind();
        if (m_upscale->valid()) {
            m_low.bindTexture(0);
            m_low.bindDepthTexture(1);
            m_upscale->use();
            m_upscale->set("uColor", 0);
            m_upscale->set("uDepth", 1);
            m_upscale->set("uResolution", resolution);
            m_resources->drawFullscreen();
        }
        glDepthFunc(GL_LESS);

        if (m_fighter->valid()) {
            glm::mat4 model(1.0f);
            model[0] = glm::vec4(ship.side, 0.0f);
            model[1] = glm::vec4(ship.up, 0.0f);
            model[2] = glm::vec4(ship.forward, 0.0f);
            model[3] = glm::vec4(ship.position, 1.0f);
            const Instance instance{model, glm::vec4(1.0f)};
            m_instanceBuffer.upload(&instance, sizeof(instance));
            m_instanceBuffer.bind(0);
            m_resources->bindNoiseVolume(0);
            m_fighter->use();
            m_fighter->set("uNoise", 0);
            m_fighter->set("uShadowMap", 5);  // unused here; keep it off the noise volume's unit
            m_fighter->set("uViewProj", viewProj);
            m_fighter->set("uCamPos", camera.position);
            m_fighter->set("uTime", static_cast<float>(t));
            m_fighter->set("uLightDir", lightDir);
            m_fighter->set("uSunColor", sunColor);
            m_fighter->set("uEngineColor", glm::vec3(0.35f, 0.6f, 1.0f));
            m_fighter->set("uEngineGlow", 14.0f * (1.0f + 0.4f * context.sync.beatPulse(6.0f)));
            m_ship.draw(1);
        }
        glDisable(GL_DEPTH_TEST);

        const glm::ivec2 size = m_frame.size();
        glBlitNamedFramebuffer(m_frame.fbo(), target.fbo(), 0, 0, size.x, size.y, 0, 0, size.x, size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        target.bind();
    }

    void imgui() override {
        ImGui::SliderFloat("speed", &m_speed, 0.0f, 120.0f);
        ImGui::SliderFloat("camera lag", &m_lag, 0.0f, 1.0f, "%.2f s");
        ImGui::SliderFloat("bank", &m_bank, 0.0f, 3.0f);
        ImGui::SliderFloat("camera height", &m_cameraHeight, 0.0f, 10.0f);
        ImGui::SliderFloat("camera back", &m_cameraBack, 0.0f, 25.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
        ImGui::SliderFloat("sun", &m_sunIntensity, 0.0f, 8.0f);
        ImGui::SliderFloat("dust", &m_dust, 0.0f, 5.0f);
        ImGui::SliderFloat("haze", &m_haze, 0.0f, 5.0f);
        ImGui::SliderInt("steps", &m_steps, 32, 400);
        ImGui::SliderFloat("terrain scale", &m_scale, 0.25f, 1.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_terrain = nullptr;
    ShaderProgram* m_upscale = nullptr;
    ShaderProgram* m_fighter = nullptr;
    Mesh m_ship;
    StorageBuffer m_instanceBuffer;
    RenderTarget m_low{GL_RGBA16F, true};
    RenderTarget m_frame{GL_RGBA16F, true};

    float m_speed = 45.0f;
    float m_lag = 0.3f;
    float m_bank = 1.0f;
    float m_cameraHeight = 3.0f;
    float m_cameraBack = 9.0f;
    float m_focal = 1.0f;
    float m_sunIntensity = 3.0f;
    float m_dust = 0.45f;
    float m_haze = 1.0f;
    int m_steps = 180;
    float m_scale = 0.6f;
};

}  // namespace

REGISTER_SCENE("canyon", CanyonScene);

}  // namespace demo

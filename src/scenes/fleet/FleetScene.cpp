// Scene 9 — fleet jump-in: an empty sky over the planet, then twelve dreadnoughts drop out of hyperspace one
// after another on the beat — each shooting in stretched along its length, braking hard into its slot in a wedge
// formation, with a streak and an arrival flash. Reuses the dreadnought's mesh, hull and sky shaders.
// Timeline params: first (beat of the first arrival), every (beats between arrivals), sunaz / sunel.

#include "core/Resources.h"
#include "core/Shader.h"
#include "gfx/Mesh.h"
#include "gfx/ShadowMap.h"
#include "scene/Camera.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"
#include "scenes/dreadnought/DreadnoughtModel.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <memory>
#include <string>

namespace demo {

namespace {

constexpr int kShips = 12;
constexpr int kCascades = 2;
constexpr int kShadowMapSize = 2048;
constexpr float kCruiseSpeed = 8.0f;
constexpr const char* kLightMatrixNames[kCascades] = {"uLightMatrix[0]", "uLightMatrix[1]"};
constexpr const char* kCascadeNames[kCascades] = {"uCascade[0]", "uCascade[1]"};

// Wedge formation: lead ship, then widening rows behind (x across, y up, z along the flight).
constexpr std::array<glm::vec3, kShips> kSlots = {{
    {0.0f, 0.0f, 0.0f},
    {260.0f, -30.0f, -230.0f}, {-260.0f, -30.0f, -230.0f},
    {0.0f, 40.0f, -380.0f}, {520.0f, -60.0f, -460.0f}, {-520.0f, -60.0f, -460.0f},
    {260.0f, 10.0f, -610.0f}, {-260.0f, 10.0f, -610.0f}, {780.0f, -90.0f, -690.0f}, {-780.0f, -90.0f, -690.0f},
    {520.0f, -20.0f, -840.0f}, {-520.0f, -20.0f, -840.0f},
}};

struct Instance {
    glm::mat4 model;
    glm::vec4 color;
};

class FleetScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_sky = &shaders.loadFullscreen("scene/fleet/sky", "scenes/dreadnought/sky.frag");
        m_hull = &shaders.load("scene/fleet/hull", {{GL_VERTEX_SHADER, "common/mesh.vert"}, {GL_FRAGMENT_SHADER, "scenes/dreadnought/hull.frag"}});
        m_shadow = &shaders.load("scene/fleet/shadow", {{GL_VERTEX_SHADER, "common/shadow.vert"}, {GL_FRAGMENT_SHADER, "common/shadow.frag"}});
        m_flashes = &shaders.loadFullscreen("scene/fleet/flashes", "scenes/fleet/flashes.frag");
        m_ship = Mesh(makeDreadnought());
        m_shadowMap = std::make_unique<ShadowMap>(kShadowMapSize, kCascades);
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const SceneParams& params = context.params;
        const double t = context.localTime;
        const double beat = 60.0 / context.sync.bpm;
        const double firstArrival = params.get("first", 4.0f) * beat;
        const double spacing = params.get("every", 2.0f) * beat;

        // Slow drift alongside the formation, slightly above it so the planet shows below.
        const float s = static_cast<float>(motion::smoothstep(0.0, 1.0, t / context.duration));
        const glm::vec3 drift(0.0f, 0.0f, kCruiseSpeed * static_cast<float>(t));
        const Camera camera = Camera::lookAt(glm::mix(glm::vec3(1100.0f, 150.0f, 600.0f), glm::vec3(950.0f, 120.0f, 300.0f), s) + drift,
                                             glm::vec3(0.0f, -80.0f, -350.0f) + drift, m_focal);
        const glm::vec2 resolution(context.size);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        const glm::mat4 projection = glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, 1.0f, 30000.0f);
        const glm::mat4 viewProj = projection * view;

        const float azimuth = glm::radians(params.get("sunaz", 70.0f));
        const float elevation = glm::radians(params.get("sunel", 18.0f));
        const glm::vec3 lightDir = glm::normalize(glm::vec3(std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
                                                            std::cos(azimuth) * std::cos(elevation)));
        const glm::vec3 sunColor(1.0f, 0.95f, 0.88f);

        // Ships that have arrived (or are arriving): braking from hyperspace, stretched along their length.
        std::array<Instance, kShips> instances{};
        int visible = 0;
        std::array<glm::vec4, kShips> streaks{};
        std::array<glm::vec2, kShips> flashes{}, flashPositions{};
        auto toPixels = [&](const glm::vec3& p, bool& onScreen) {
            const glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
            onScreen = clip.w > 0.0f;
            return (glm::vec2(clip) / std::max(clip.w, 1e-3f) * 0.5f + 0.5f) * resolution;
        };
        for (int i = 0; i < kShips; ++i) {
            const double since = t - (firstArrival + i * spacing);
            if (since < 0.0) continue;
            const glm::vec3 slot = kSlots[i] + drift;
            const float approach = static_cast<float>(2000.0 * std::exp(-since * 7.0));
            const float stretch = 1.0f + static_cast<float>(25.0 * std::exp(-since * 10.0));
            const glm::vec3 position = slot - glm::vec3(0.0f, 0.0f, approach);
            instances[visible++] = {glm::scale(glm::translate(glm::mat4(1.0f), position), glm::vec3(1.0f, 1.0f, stretch)), glm::vec4(1.0f)};

            bool tailVisible = false, headVisible = false;
            const glm::vec2 tail = toPixels(position - glm::vec3(0.0f, 0.0f, 120.0f * stretch), tailVisible);
            const glm::vec2 head = toPixels(position + glm::vec3(0.0f, 0.0f, 100.0f), headVisible);
            streaks[i] = glm::vec4(tail, head);
            flashes[i] = glm::vec2(tailVisible && headVisible ? static_cast<float>(std::exp(-since * 6.0)) * 3.0f : 0.0f,
                                   headVisible ? static_cast<float>(std::exp(-since * 4.0)) * 4.0f : 0.0f);
            flashPositions[i] = head;
        }

        const std::array<glm::vec4, kCascades> cascades = {glm::vec4(glm::vec3(0.0f, -40.0f, -420.0f) + drift, 700.0f),
                                                           glm::vec4(glm::vec3(0.0f, -40.0f, -420.0f) + drift, 1400.0f)};
        std::array<glm::mat4, kCascades> lightMatrices;
        for (int c = 0; c < kCascades; ++c)
            lightMatrices[c] = fitDirectionalShadow(lightDir, glm::vec3(cascades[c]), cascades[c].w, kShadowMapSize, 400.0f);

        if (visible > 0) {
            m_instanceBuffer.upload(instances.data(), visible * sizeof(Instance));
            m_instanceBuffer.bind(0);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        if (m_shadow->valid() && visible > 0) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.0f, 4.0f);
            m_shadow->use();
            for (int c = 0; c < kCascades; ++c) {
                m_shadowMap->beginCascade(c);
                m_shadow->set("uLightViewProj", lightMatrices[c]);
                m_ship.draw(visible);
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        m_frame.resize(context.size);
        m_frame.bind();
        m_frame.clear();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        if (m_sky->valid()) {
            m_sky->use();
            camera.apply(*m_sky);
            m_sky->set("uResolution", resolution);
            m_sky->set("uTime", static_cast<float>(t));
            m_sky->set("uLightDir", lightDir);
            m_sky->set("uSunColor", sunColor * 3.0f);
            m_sky->set("uPlanetCenter", glm::vec3(0.0f, -20450.0f, 0.0f));
            m_sky->set("uPlanetRadius", 20000.0f);
            m_sky->set("uAtmosphere", 60.0f);
            m_sky->set("uNightLights", 1.5f);
            m_resources->drawFullscreen();
        }
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        if (m_hull->valid() && visible > 0) {
            m_shadowMap->bindTexture(0);
            m_hull->use();
            m_hull->set("uViewProj", viewProj);
            m_hull->set("uCamPos", camera.position);
            m_hull->set("uTime", static_cast<float>(t));
            m_hull->set("uLightDir", lightDir);
            m_hull->set("uLightColor", sunColor * 2.4f);
            m_hull->set("uShadowMap", 0);
            for (int c = 0; c < kCascades; ++c) {
                m_hull->set(kLightMatrixNames[c], lightMatrices[c]);
                m_hull->set(kCascadeNames[c], cascades[c]);
            }
            m_hull->set("uShadowTexel", 1.0f / kShadowMapSize);
            m_hull->set("uShadowSoftness", 1.5f);
            m_hull->set("uAmbient", 0.01f);
            m_hull->set("uBounce", 0.6f);
            m_hull->set("uReflect", 0.8f);
            m_hull->set("uPanelScale", 1.0f);
            m_hull->set("uEngineColor", glm::vec3(0.45f, 0.65f, 1.0f));
            m_hull->set("uEngineGlow", 15.0f * (1.0f + 0.3f * context.sync.beatPulse(5.0f)));
            m_hull->set("uEnginePos", glm::vec3(0.0f, 1e5f, 0.0f));  // no single stern to light here
            m_hull->set("uWindowGlow", 2.5f);
            m_ship.draw(visible);
        }
        glDisable(GL_DEPTH_TEST);

        if (m_flashes->valid()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            m_flashes->use();
            m_flashes->set("uResolution", resolution);
            m_flashes->set("uShipCount", kShips);
            for (int i = 0; i < kShips; ++i) {
                m_flashes->set(std::format("uStreak[{}]", i).c_str(), streaks[i]);
                m_flashes->set(std::format("uFlash[{}]", i).c_str(), flashes[i]);
                m_flashes->set(std::format("uFlashPos[{}]", i).c_str(), flashPositions[i]);
            }
            m_resources->drawFullscreen();
            glDisable(GL_BLEND);
        }

        const glm::ivec2 size = m_frame.size();
        glBlitNamedFramebuffer(m_frame.fbo(), target.fbo(), 0, 0, size.x, size.y, 0, 0, size.x, size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        target.bind();
    }

    void imgui() override { ImGui::SliderFloat("focal", &m_focal, 0.5f, 4.0f); }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_sky = nullptr;
    ShaderProgram* m_hull = nullptr;
    ShaderProgram* m_shadow = nullptr;
    ShaderProgram* m_flashes = nullptr;
    Mesh m_ship;
    std::unique_ptr<ShadowMap> m_shadowMap;
    StorageBuffer m_instanceBuffer;
    RenderTarget m_frame{GL_RGBA16F, true};
    float m_focal = 1.4f;
};

}  // namespace

REGISTER_SCENE("fleet", FleetScene);

}  // namespace demo

// Scene 10 — dreadnought patrol: a wedge-shaped capital ship cruising in low orbit over a planet, seen from a
// tiny craft. The ship does the moving: in most shots the camera hangs almost still while the giant hull slides
// through the frame (overhead pass, planet eclipse, departure); in one it drifts slowly over the decks.
// Shots are cut hard, each taking its weight's share of the entry (equal weights: cuts on bar lines).
// Rasterized hull with cascaded soft self-shadows from a low grazing sun; raytraced planet and sky behind.
// Timeline params: shot (-1 = sequence, 0..3 = hold one shot), speed (ship speed multiplier), sunaz / sunel
//                  (degrees), ambient, bounce, reflect, softness, engines, windows (all optional; defaults are
//                  the overlay slider values).

#include "core/Resources.h"
#include "core/Shader.h"
#include "gfx/Mesh.h"
#include "gfx/ShadowMap.h"
#include "scene/Camera.h"
#include "scene/SceneRegistry.h"
#include "scenes/dreadnought/DreadnoughtModel.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace demo {

namespace {

constexpr int kCascades = 2;
constexpr int kShadowMapSize = 2048;
constexpr float kNearPlane = 0.5f;
constexpr float kFarPlane = 5000.0f;
constexpr const char* kLightMatrixNames[kCascades] = {"uLightMatrix[0]", "uLightMatrix[1]"};
constexpr const char* kCascadeNames[kCascades] = {"uCascade[0]", "uCascade[1]"};

struct Instance {
    glm::mat4 model;
    glm::vec4 color;
};

// One shot of the patrol. The ship cruises along +z from `shipStart` at `shipSpeed`. Camera position and aim
// interpolate from -> to over the shot, each either in world space (hanging still while the ship passes) or in
// the ship's frame (riding along with it).
struct Shot {
    const char* name;
    float weight;  // share of the entry
    float shipStart;
    float shipSpeed;
    bool cameraInShipFrame;
    glm::vec3 from, to;
    bool aimInShipFrame;
    glm::vec3 aimFrom, aimTo;
    float focal;
};

constexpr std::array<Shot, 4> kShots = {{
    // Below and ahead, looking slightly up with the planet horizon at the bottom: the nose slides in over the
    // top of the frame and the belly slowly fills the view.
    {"overhead pass", 1.0f, -35.0f, 9.0f,
     false, {-14.0f, -34.0f, 60.0f}, {-12.0f, -33.0f, 64.0f},
     false, {0.0f, -12.0f, 160.0f}, {2.0f, -10.0f, 164.0f}, 0.9f},
    // Far to the side and a little above: the hull crosses the frame and eclipses the planet's bright limb.
    {"planet eclipse", 1.0f, -230.0f, 22.0f,
     false, {160.0f, 40.0f, 0.0f}, {158.0f, 38.0f, 8.0f},
     false, {0.0f, -5.0f, 0.0f}, {0.0f, -5.0f, 8.0f}, 1.2f},
    // Riding just above the decks, slipping slowly backward over the city toward the looming command tower.
    {"deck drift", 1.0f, 0.0f, 9.0f,
     true, {12.0f, 26.0f, 10.0f}, {9.0f, 27.0f, -24.0f},
     true, {0.0f, 34.0f, -83.0f}, {0.0f, 34.0f, -83.0f}, 1.0f},
    // Hanging beside the stern: the engines pass and recede, the camera turning to follow them.
    {"departure", 1.0f, 120.0f, 12.0f,
     false, {75.0f, 8.0f, 40.0f}, {72.0f, 10.0f, 46.0f},
     true, {0.0f, 0.0f, -110.0f}, {0.0f, 0.0f, -110.0f}, 1.1f},
}};

float easeInOut(float x) { return x * x * (3.0f - 2.0f * x); }

class DreadnoughtScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_skyProgram = &shaders.loadFullscreen("scene/dreadnought/sky", "scenes/dreadnought/sky.frag");
        m_hullProgram = &shaders.load("scene/dreadnought/hull", {{GL_VERTEX_SHADER, "common/mesh.vert"},
                                                                 {GL_FRAGMENT_SHADER, "scenes/dreadnought/hull.frag"}});
        m_shadowProgram = &shaders.load("scene/dreadnought/shadow", {{GL_VERTEX_SHADER, "common/shadow.vert"},
                                                                     {GL_FRAGMENT_SHADER, "common/shadow.frag"}});
        const MeshData model = makeDreadnought();
        m_triangles = static_cast<int>(model.indices.size() / 3);
        m_ship = Mesh(model);
        m_shadowMap = std::make_unique<ShadowMap>(kShadowMapSize, kCascades);
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const SceneParams& params = context.params;
        const double t = context.localTime;

        // Shot selection: each shot gets its weight's share of the entry, or one shot is held (and loops).
        const int forced = static_cast<int>(params.get("shot", static_cast<float>(m_shot)));
        float totalWeight = 0.0f;
        for (const Shot& candidate : kShots) totalWeight += candidate.weight;
        int index = static_cast<int>(kShots.size()) - 1;
        double shotStart = 0.0;
        for (int i = 0; i < static_cast<int>(kShots.size()); ++i) {
            const double length = context.duration * kShots[i].weight / totalWeight;
            if (forced < 0 ? t < shotStart + length : i == std::min(forced, static_cast<int>(kShots.size()) - 1)) {
                index = i;
                break;
            }
            if (forced < 0) shotStart += length;
        }
        const Shot& shot = kShots[index];
        const double shotLength = context.duration * shot.weight / totalWeight;
        const double shotTime = forced >= 0 ? std::fmod(t, shotLength) : std::clamp(t - shotStart, 0.0, shotLength);
        const float s = easeInOut(static_cast<float>(shotTime / shotLength));
        m_currentShot = index;

        // The ship cruises; the camera hangs in space or rides along, depending on the shot.
        const float shipZ = shot.shipStart + shot.shipSpeed * params.get("speed", m_speed) * static_cast<float>(shotTime);
        const glm::vec3 shipOffset(0.0f, 0.0f, shipZ);
        const glm::vec3 cameraPos = glm::mix(shot.from, shot.to, s) + (shot.cameraInShipFrame ? shipOffset : glm::vec3(0.0f));
        const glm::vec3 aim = glm::mix(shot.aimFrom, shot.aimTo, s) + (shot.aimInShipFrame ? shipOffset : glm::vec3(0.0f));
        const Camera camera = Camera::lookAt(cameraPos, aim, shot.focal * m_zoom);

        const glm::vec2 resolution(context.size);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        const glm::mat4 projection =
            glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, kNearPlane, kFarPlane);
        const glm::mat4 viewProj = projection * view;

        const float azimuth = glm::radians(params.get("sunaz", m_sunAzimuth));
        const float elevation = glm::radians(params.get("sunel", m_sunElevation));
        const glm::vec3 lightDir = glm::normalize(glm::vec3(std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
                                                            std::cos(azimuth) * std::cos(elevation)));
        const glm::vec3 sunColor(1.0f, 0.95f, 0.88f);

        // Cascades: a tight one just in front of the camera, a wide one over the whole ship.
        const std::array<glm::vec4, kCascades> cascades = {glm::vec4(camera.position + camera.forward * 40.0f, 45.0f),
                                                           glm::vec4(shipOffset + glm::vec3(0.0f, 5.0f, -10.0f), 130.0f)};
        std::array<glm::mat4, kCascades> lightMatrices;
        for (int c = 0; c < kCascades; ++c)
            lightMatrices[c] = fitDirectionalShadow(lightDir, glm::vec3(cascades[c]), cascades[c].w, kShadowMapSize, 150.0f);

        const Instance instance{glm::translate(glm::mat4(1.0f), shipOffset), glm::vec4(1.0f)};
        m_instanceBuffer.upload(&instance, sizeof(instance));
        m_instanceBuffer.bind(0);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        if (m_shadowProgram->valid()) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.0f, 4.0f);
            m_shadowProgram->use();
            for (int c = 0; c < kCascades; ++c) {
                m_shadowMap->beginCascade(c);
                m_shadowProgram->set("uLightViewProj", lightMatrices[c]);
                m_ship.draw(1);
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        m_frame.resize(context.size);
        m_frame.bind();
        m_frame.clear();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        if (m_skyProgram->valid()) {
            m_skyProgram->use();
            camera.apply(*m_skyProgram);
            m_skyProgram->set("uResolution", resolution);
            m_skyProgram->set("uTime", static_cast<float>(t));
            m_skyProgram->set("uLightDir", lightDir);
            m_skyProgram->set("uSunColor", sunColor * 3.0f);
            m_skyProgram->set("uPlanetCenter", glm::vec3(0.0f, -20450.0f, 0.0f));
            m_skyProgram->set("uPlanetRadius", 20000.0f);
            m_skyProgram->set("uAtmosphere", 60.0f);
            m_skyProgram->set("uNightLights", m_nightLights);
            m_resources->drawFullscreen();
        }
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        if (m_hullProgram->valid()) {
            m_shadowMap->bindTexture(0);
            m_hullProgram->use();
            m_hullProgram->set("uViewProj", viewProj);
            m_hullProgram->set("uCamPos", camera.position);
            m_hullProgram->set("uTime", static_cast<float>(t));
            m_hullProgram->set("uLightDir", lightDir);
            m_hullProgram->set("uLightColor", sunColor * m_sunIntensity);
            m_hullProgram->set("uShadowMap", 0);
            for (int c = 0; c < kCascades; ++c) {
                m_hullProgram->set(kLightMatrixNames[c], lightMatrices[c]);
                m_hullProgram->set(kCascadeNames[c], cascades[c]);
            }
            m_hullProgram->set("uShadowTexel", 1.0f / kShadowMapSize);
            m_hullProgram->set("uShadowSoftness", params.get("softness", m_softness));
            m_hullProgram->set("uAmbient", params.get("ambient", m_ambient));
            m_hullProgram->set("uBounce", params.get("bounce", m_bounce));
            m_hullProgram->set("uReflect", params.get("reflect", m_reflect));
            m_hullProgram->set("uPanelScale", m_panelScale);
            m_hullProgram->set("uEngineColor", glm::vec3(0.45f, 0.65f, 1.0f));
            m_hullProgram->set("uEngineGlow", params.get("engines", m_engineGlow) * (1.0f + 0.3f * context.sync.beatPulse(5.0f)));
            m_hullProgram->set("uEnginePos", kDreadnoughtEngineCenter + shipOffset);
            m_hullProgram->set("uWindowGlow", params.get("windows", m_windowGlow));
            m_ship.draw(1);
        }
        glDisable(GL_DEPTH_TEST);

        const glm::ivec2 size = m_frame.size();
        glBlitNamedFramebuffer(m_frame.fbo(), target.fbo(), 0, 0, size.x, size.y, 0, 0, size.x, size.y,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);
        target.bind();
    }

    void imgui() override {
        ImGui::Text("%d triangles, shot %d: %s", m_triangles, m_currentShot, kShots[m_currentShot].name);
        ImGui::SliderInt("hold shot", &m_shot, -1, static_cast<int>(kShots.size()) - 1, m_shot < 0 ? "sequence" : "%d");
        ImGui::SliderFloat("zoom", &m_zoom, 0.5f, 3.0f);
        ImGui::SliderFloat("ship speed", &m_speed, 0.0f, 3.0f, "%.2fx");
        ImGui::SeparatorText("Light");
        ImGui::SliderFloat("sun azimuth", &m_sunAzimuth, -180.0f, 180.0f, "%.0f deg");
        ImGui::SliderFloat("sun elevation", &m_sunElevation, -30.0f, 90.0f, "%.0f deg");
        ImGui::SliderFloat("sun intensity", &m_sunIntensity, 0.5f, 10.0f);
        ImGui::SliderFloat("shadow softness", &m_softness, 0.0f, 8.0f, "%.1f texels");
        ImGui::SliderFloat("ambient", &m_ambient, 0.0f, 0.3f);
        ImGui::SliderFloat("planet bounce", &m_bounce, 0.0f, 3.0f);
        ImGui::SliderFloat("reflection", &m_reflect, 0.0f, 3.0f);
        ImGui::SeparatorText("Surface");
        ImGui::SliderFloat("panel scale", &m_panelScale, 0.25f, 4.0f);
        ImGui::SliderFloat("engine glow", &m_engineGlow, 0.0f, 40.0f);
        ImGui::SliderFloat("window lights", &m_windowGlow, 0.0f, 10.0f);
        ImGui::SliderFloat("night lights", &m_nightLights, 0.0f, 5.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_skyProgram = nullptr;
    ShaderProgram* m_hullProgram = nullptr;
    ShaderProgram* m_shadowProgram = nullptr;
    Mesh m_ship;
    int m_triangles = 0;
    std::unique_ptr<ShadowMap> m_shadowMap;
    StorageBuffer m_instanceBuffer;
    RenderTarget m_frame{GL_RGBA16F, true};
    int m_currentShot = 0;

    int m_shot = -1;
    float m_zoom = 1.0f;
    float m_speed = 1.0f;
    float m_sunAzimuth = 70.0f;
    float m_sunElevation = 18.0f;
    float m_sunIntensity = 2.4f;
    float m_softness = 1.5f;
    float m_ambient = 0.01f;
    float m_bounce = 0.6f;
    float m_reflect = 0.8f;
    float m_panelScale = 1.0f;
    float m_engineGlow = 15.0f;
    float m_nightLights = 1.5f;
    float m_windowGlow = 2.5f;
};

}  // namespace

REGISTER_SCENE("dreadnought", DreadnoughtScene);

}  // namespace demo

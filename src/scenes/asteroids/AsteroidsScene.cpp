// Scene 6 — asteroid chase: a small fighter weaves through an asteroid field lit by a small star in a corner.
// Rasterized: procedural rock meshes (8 variants x 2 LODs) instanced from one storage buffer, cascaded soft
// shadow maps (rocks shadow each other and the ship), near-zero ambient for hard contrast, backlit rims,
// screen-space light shafts and a lens flare.
// The path is built from the field: it skims one large rock after another, passing through each rock's
// shadow (the rock between ship and star), so shadows sweep across the hull. Other rocks keep out of the
// corridor. The camera rides the same path `lag` seconds behind, so it always seems to be catching up.
// Everything is a function of time: scrubbable and loopable.
// Timeline params: speed, density, spread (field radius), lag (s), bank, starx / stary (star direction, degrees),
//                  ambient, softness, shafts, flare (all optional; defaults are the overlay slider values).

#include "core/Resources.h"
#include "core/Shader.h"
#include "gfx/Frustum.h"
#include "gfx/Mesh.h"
#include "gfx/ShadowMap.h"
#include "scene/Camera.h"
#include "scene/SceneRegistry.h"
#include "scenes/asteroids/Fighter.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

namespace demo {

namespace {

constexpr int kVariants = 8;
constexpr int kLods = 2;            // 0 = 5120 triangles, 1 = 320 triangles
constexpr int kCascades = 2;
constexpr int kShadowMapSize = 2048;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 2500.0f;
constexpr float kCasterReach = 400.0f;  // how far toward the star occluders still cast into a cascade
constexpr const char* kLightMatrixNames[kCascades] = {"uLightMatrix[0]", "uLightMatrix[1]"};
constexpr const char* kCascadeNames[kCascades] = {"uCascade[0]", "uCascade[1]"};

struct Instance {
    glm::mat4 model;
    glm::vec4 color;
};
static_assert(sizeof(Instance) == 80, "must match the std430 Instance in shaders/common/mesh.vert");

struct Asteroid {
    glm::vec3 position;
    float size;
    glm::vec3 axis;
    float spin;
    float phase;
    int variant;
    glm::vec3 albedo;
};

struct Pose {
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 up;
    glm::vec3 right;
};

struct DrawBatch {
    const Mesh* mesh;
    GLuint first;
    GLsizei count;
};

// Flight path through waypoints sorted by z: x and y are smooth (cubic Hermite, Catmull-Rom tangents)
// functions of the distance z along the flight direction.
class FlightPath {
public:
    void set(std::vector<glm::vec3> waypoints) { m_points = std::move(waypoints); }

    glm::vec3 at(double z) const {
        if (m_points.size() < 2) return {0.0f, 0.0f, static_cast<float>(z)};
        const auto it = std::upper_bound(m_points.begin(), m_points.end(), z,
                                         [](double value, const glm::vec3& p) { return value < p.z; });
        const size_t i = static_cast<size_t>(std::clamp<std::ptrdiff_t>(it - m_points.begin() - 1, 0,
                                                                        static_cast<std::ptrdiff_t>(m_points.size()) - 2));
        const glm::vec3& a = m_points[i];
        const glm::vec3& b = m_points[i + 1];
        const float span = b.z - a.z;
        const float t = std::clamp(static_cast<float>((z - a.z) / span), 0.0f, 1.0f);
        const float t2 = t * t, t3 = t2 * t;
        const glm::vec2 xy = glm::vec2(a) * (2.0f * t3 - 3.0f * t2 + 1.0f) + tangent(i) * span * (t3 - 2.0f * t2 + t) +
                             glm::vec2(b) * (-2.0f * t3 + 3.0f * t2) + tangent(i + 1) * span * (t3 - t2);
        return {xy, static_cast<float>(z)};
    }

private:
    glm::vec2 tangent(size_t i) const {  // d(xy)/dz from the neighbors
        const size_t previous = i == 0 ? 0 : i - 1;
        const size_t next = std::min(i + 1, m_points.size() - 1);
        return (glm::vec2(m_points[next]) - glm::vec2(m_points[previous])) / (m_points[next].z - m_points[previous].z);
    }

    std::vector<glm::vec3> m_points;
};

// Position and orientation on the path at distance z; banks into turns in proportion to the sideways
// acceleration at the given speed.
Pose poseAt(const FlightPath& path, double z, double speed, float bankGain) {
    const double h = 1.0;
    const glm::vec3 p = path.at(z), previous = path.at(z - h), next = path.at(z + h);
    const glm::vec3 forward = glm::normalize(next - previous);
    const glm::vec3 acceleration = (next - 2.0f * p + previous) / static_cast<float>(h * h) * static_cast<float>(speed * speed);
    const glm::vec3 lateral = acceleration - forward * glm::dot(acceleration, forward);
    const glm::vec3 lean = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) + lateral * bankGain);
    const glm::vec3 right = glm::normalize(glm::cross(forward, lean));
    return {p, forward, glm::cross(right, forward), right};
}

glm::mat4 lightViewMatrix(const glm::vec3& lightDir) {
    const glm::vec3 up = std::abs(lightDir.y) > 0.95f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    return glm::lookAt(glm::vec3(0.0f), -lightDir, up);  // looking along the light's travel
}

class AsteroidsScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_skyProgram = &shaders.loadFullscreen("scene/asteroids/sky", "scenes/asteroids/sky.frag");
        m_rockProgram = &shaders.load("scene/asteroids/rock", {{GL_VERTEX_SHADER, "common/mesh.vert"},
                                                               {GL_FRAGMENT_SHADER, "scenes/asteroids/rock.frag"}});
        m_shipProgram = &shaders.load("scene/asteroids/ship", {{GL_VERTEX_SHADER, "common/mesh.vert"},
                                                               {GL_FRAGMENT_SHADER, "scenes/asteroids/ship.frag"}});
        m_shadowProgram = &shaders.load("scene/asteroids/shadow", {{GL_VERTEX_SHADER, "common/shadow.vert"},
                                                                   {GL_FRAGMENT_SHADER, "common/shadow.frag"}});
        m_shaftsProgram = &shaders.loadFullscreen("scene/asteroids/shafts", "scenes/asteroids/shafts.frag");
        m_compositeProgram = &shaders.loadFullscreen("scene/asteroids/composite", "scenes/asteroids/composite.frag");

        for (int v = 0; v < kVariants; ++v) {
            const auto seed = 1000u + static_cast<std::uint32_t>(v) * 7919u;
            m_rocks[v][0] = Mesh(makeAsteroid(seed, 4));
            m_rocks[v][1] = Mesh(makeAsteroid(seed, 2));
        }
        m_ship = Mesh(makeFighter());
        m_shadowMap = std::make_unique<ShadowMap>(kShadowMapSize, kCascades);
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const SceneParams& params = context.params;
        const double t = context.localTime;
        const float speed = m_speed * params.get("speed", 1.0f);

        // Light: the small star, placed by azimuth/elevation relative to the flight direction (+z).
        const float azimuth = glm::radians(params.get("starx", m_starAzimuth));
        const float elevation = glm::radians(params.get("stary", m_starElevation));
        const glm::vec3 lightDir = glm::normalize(glm::vec3(std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
                                                            std::cos(azimuth) * std::cos(elevation)));

        ensureField(context.duration, speed, params.get("density", m_density), params.get("spread", m_spread), lightDir);

        // Ship and chase camera.
        const float bankGain = 0.04f * params.get("bank", m_bank);
        const Pose ship = poseAt(m_path, speed * t, speed, bankGain);
        const Pose chase = poseAt(m_path, speed * (t - params.get("lag", m_lag)), speed, bankGain * 0.3f);
        Camera camera;
        camera.position = chase.position + chase.up * m_cameraHeight - chase.forward * m_cameraBack;
        camera.forward = glm::normalize(ship.position + ship.forward * 6.0f - camera.position);
        camera.right = glm::normalize(glm::cross(camera.forward, chase.up));
        camera.up = glm::cross(camera.right, camera.forward);
        camera.focal = m_focal;

        const glm::vec2 resolution(context.size);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        const glm::mat4 projection =
            glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, kNearPlane, kFarPlane);
        const glm::mat4 viewProj = projection * view;

        // Cascades: a tight one around the ship, a wide one over the visible field.
        const glm::mat4 lightView = lightViewMatrix(lightDir);
        const std::array<glm::vec4, kCascades> cascades = {glm::vec4(ship.position, 28.0f),
                                                           glm::vec4(camera.position + camera.forward * 150.0f, 190.0f)};
        std::array<glm::mat4, kCascades> lightMatrices;
        std::array<glm::vec3, kCascades> lightCenters;
        for (int c = 0; c < kCascades; ++c) {
            const float radius = cascades[c].w;
            glm::vec3 center = glm::vec3(lightView * glm::vec4(glm::vec3(cascades[c]), 1.0f));
            const float texel = 2.0f * radius / kShadowMapSize;
            center.x = std::floor(center.x / texel) * texel;  // snap to texels: no shimmering while moving
            center.y = std::floor(center.y / texel) * texel;
            lightCenters[c] = center;
            lightMatrices[c] = glm::ortho(center.x - radius, center.x + radius, center.y - radius, center.y + radius,
                                          -center.z - radius - kCasterReach, -center.z + radius) * lightView;
        }

        buildInstances(t, ship, camera.position, Frustum::fromMatrix(viewProj), lightView, lightCenters, cascades);
        m_instanceBuffer.upload(m_instances.data(), m_instances.size() * sizeof(Instance));
        m_instanceBuffer.bind(0);

        // Shadow maps.
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        if (m_shadowProgram->valid()) {
            m_shadowProgram->use();
            for (int c = 0; c < kCascades; ++c) {
                m_shadowMap->beginCascade(c);
                m_shadowProgram->set("uLightViewProj", lightMatrices[c]);
                for (const DrawBatch& batch : m_shadowBatches[c]) batch.mesh->draw(batch.count, batch.first);
                m_ship.draw(1, m_shipInstance);
            }
        }
        glDisable(GL_POLYGON_OFFSET_FILL);

        // Main pass: sky first (no depth writes, so depth stays 1.0 where the sky shows), then meshes.
        m_frame.resize(context.size);
        m_frame.bind();
        m_frame.clear();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        const glm::vec3 starColor(1.0f, 0.9f, 0.75f);
        if (m_skyProgram->valid()) {
            m_skyProgram->use();
            camera.apply(*m_skyProgram);
            m_skyProgram->set("uResolution", resolution);
            m_skyProgram->set("uLightDir", lightDir);
            m_skyProgram->set("uStarColor", starColor);
            m_skyProgram->set("uStarSize", glm::radians(m_starSize));
            m_resources->drawFullscreen();
        }
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        m_shadowMap->bindTexture(0);

        const float ambient = params.get("ambient", m_ambient);
        const float softness = params.get("softness", m_softness);
        auto setLighting = [&](ShaderProgram& program) {
            program.set("uViewProj", viewProj);
            program.set("uCamPos", camera.position);
            program.set("uLightDir", lightDir);
            program.set("uLightColor", starColor * m_lightIntensity);
            program.set("uShadowMap", 0);
            for (int c = 0; c < kCascades; ++c) {
                program.set(kLightMatrixNames[c], lightMatrices[c]);
                program.set(kCascadeNames[c], cascades[c]);
            }
            program.set("uShadowTexel", 1.0f / kShadowMapSize);
            program.set("uShadowSoftness", softness);
            program.set("uAmbient", ambient);
        };

        if (m_rockProgram->valid()) {
            glEnable(GL_CULL_FACE);  // rocks are closed and outward-facing
            glCullFace(GL_BACK);
            m_rockProgram->use();
            setLighting(*m_rockProgram);
            m_rockProgram->set("uBump", m_bump);
            m_rockProgram->set("uRim", m_rim);
            for (const DrawBatch& batch : m_mainBatches) batch.mesh->draw(batch.count, batch.first);
            glDisable(GL_CULL_FACE);
        }
        if (m_shipProgram->valid()) {
            m_shipProgram->use();
            setLighting(*m_shipProgram);
            m_shipProgram->set("uTime", static_cast<float>(t));
            m_shipProgram->set("uEngineColor", glm::vec3(0.35f, 0.6f, 1.0f));
            m_shipProgram->set("uEngineGlow", m_engineGlow * (1.0f + 0.4f * context.sync.beatPulse(6.0f)));
            m_ship.draw(1, m_shipInstance);
        }
        glDisable(GL_DEPTH_TEST);

        // Star position on screen for the shafts and the flare.
        const glm::vec4 starClip = viewProj * glm::vec4(lightDir, 0.0f);
        const float inFront = starClip.w > 0.0f ? 1.0f : 0.0f;
        const glm::vec2 lightUV = inFront > 0.0f ? glm::vec2(starClip) / starClip.w * 0.5f + 0.5f : glm::vec2(-10.0f);

        m_shafts.resize(glm::max(context.size / 2, glm::ivec2(1)));
        m_shafts.bind();
        if (m_shaftsProgram->valid()) {
            m_frame.bindDepthTexture(0);
            m_shaftsProgram->use();
            m_shaftsProgram->set("uDepth", 0);
            m_shaftsProgram->set("uResolution", glm::vec2(m_shafts.size()));
            m_shaftsProgram->set("uLightUV", lightUV);
            m_shaftsProgram->set("uLightInFront", inFront);
            m_shaftsProgram->set("uStrength", params.get("shafts", m_shaftStrength));
            m_shaftsProgram->set("uDecay", m_shaftDecay);
            m_shaftsProgram->set("uStarColor", starColor);
            m_resources->drawFullscreen();
        } else {
            m_shafts.clear();
        }

        target.bind();
        if (m_compositeProgram->valid()) {
            m_frame.bindTexture(0);
            m_shafts.bindTexture(1);
            m_frame.bindDepthTexture(2);
            m_compositeProgram->use();
            m_compositeProgram->set("uFrame", 0);
            m_compositeProgram->set("uShafts", 1);
            m_compositeProgram->set("uDepth", 2);
            m_compositeProgram->set("uResolution", resolution);
            m_compositeProgram->set("uLightUV", lightUV);
            m_compositeProgram->set("uLightInFront", inFront);
            m_compositeProgram->set("uFlare", params.get("flare", m_flare));
            m_compositeProgram->set("uStarColor", starColor);
            m_resources->drawFullscreen();
        }
    }

    void imgui() override {
        ImGui::Text("%d rocks in field, %d drawn, %d in shadow maps", static_cast<int>(m_field.size()), m_drawnRocks,
                    m_shadowRocks);
        ImGui::SeparatorText("Flight");
        ImGui::SliderFloat("speed", &m_speed, 0.0f, 120.0f);
        ImGui::SliderFloat("density", &m_density, 0.1f, 4.0f);
        ImGui::SliderFloat("spread", &m_spread, 60.0f, 600.0f);
        ImGui::SliderFloat("corridor", &m_clearance, 2.0f, 30.0f);
        ImGui::SliderFloat("skim spacing", &m_heroSpacing, 30.0f, 200.0f);
        ImGui::SliderFloat("skim distance", &m_passDistance, 3.0f, 20.0f);
        ImGui::SliderFloat("camera lag", &m_lag, 0.0f, 1.5f, "%.2f s");
        ImGui::SliderFloat("bank", &m_bank, 0.0f, 3.0f);
        ImGui::SliderFloat("camera height", &m_cameraHeight, 0.0f, 6.0f);
        ImGui::SliderFloat("camera back", &m_cameraBack, 0.0f, 20.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
        ImGui::SeparatorText("Light");
        ImGui::SliderFloat("star azimuth", &m_starAzimuth, -80.0f, 80.0f, "%.0f deg");
        ImGui::SliderFloat("star elevation", &m_starElevation, -60.0f, 60.0f, "%.0f deg");
        ImGui::SliderFloat("star size", &m_starSize, 0.1f, 3.0f, "%.2f deg");
        ImGui::SliderFloat("intensity", &m_lightIntensity, 0.5f, 10.0f);
        ImGui::SliderFloat("ambient", &m_ambient, 0.0f, 0.5f);
        ImGui::SliderFloat("shadow softness", &m_softness, 0.0f, 8.0f, "%.1f texels");
        ImGui::SliderFloat("rim", &m_rim, 0.0f, 4.0f);
        ImGui::SliderFloat("bump", &m_bump, 0.0f, 4.0f);
        ImGui::SeparatorText("Lens");
        ImGui::SliderFloat("shafts", &m_shaftStrength, 0.0f, 4.0f);
        ImGui::SliderFloat("shaft decay", &m_shaftDecay, 0.9f, 1.0f, "%.3f");
        ImGui::SliderFloat("flare", &m_flare, 0.0f, 3.0f);
        ImGui::SliderFloat("engine glow", &m_engineGlow, 0.0f, 40.0f);
    }

private:
    // Builds the flight path and the rocks around it for the whole entry (plus the view distance ahead).
    // Regenerated only when the inputs change, so it stays deterministic.
    //  1. Hero rocks, one every ~heroSpacing units, around a slowly wandering center line.
    //  2. One waypoint per hero rock, set back from it along the light direction: there the rock sits exactly
    //     between the ship and the star, so its shadow sweeps over the hull as the ship skims past.
    //  3. A smooth path through the waypoints; hero rocks the curve swings too close to are nudged away.
    //  4. The rest of the field, kept out of the corridor around the path.
    void ensureField(double duration, float speed, float density, float spread, const glm::vec3& lightDir) {
        if (duration == m_fieldDuration && speed == m_fieldSpeed && density == m_fieldDensity &&
            spread == m_fieldSpread && m_clearance == m_fieldClearance && lightDir == m_fieldLight &&
            m_heroSpacing == m_fieldHeroSpacing && m_passDistance == m_fieldPassDistance)
            return;
        m_fieldDuration = duration;
        m_fieldSpeed = speed;
        m_fieldDensity = density;
        m_fieldSpread = spread;
        m_fieldClearance = m_clearance;
        m_fieldLight = lightDir;
        m_fieldHeroSpacing = m_heroSpacing;
        m_fieldPassDistance = m_passDistance;

        m_field.clear();
        std::mt19937 rng(20261016u);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        const double zMin = -80.0;
        const double zMax = speed * (duration + 3.0) + 900.0;

        auto randomize = [&](Asteroid& rock) {
            const glm::vec3 axis(unit(rng) * 2.0f - 1.0f, unit(rng) * 2.0f - 1.0f, unit(rng) * 2.0f - 1.0f);
            rock.axis = glm::normalize(axis + glm::vec3(0.0f, 0.01f, 0.0f));
            rock.spin = (unit(rng) - 0.5f) * 0.8f / std::sqrt(rock.size);
            rock.phase = unit(rng) * 2.0f * std::numbers::pi_v<float>;
            rock.variant = std::min(static_cast<int>(unit(rng) * kVariants), kVariants - 1);
            const float tone = unit(rng);
            const float brightness = glm::mix(0.6f, 1.0f, unit(rng));
            rock.albedo = glm::mix(glm::vec3(0.32f, 0.31f, 0.30f), glm::vec3(0.36f, 0.26f, 0.18f), tone) * brightness;
        };

        // 1 + 2: hero rocks and their shadow waypoints. Moving `back` units against the light shifts sideways by
        // lateral * back, so `back` is chosen to leave `passDistance` between the rock surface and the ship.
        const float lateral = std::max(glm::length(glm::vec2(lightDir)), 0.3f);
        std::vector<glm::vec3> waypoints = {{0.0f, 0.0f, static_cast<float>(zMin - 200.0)}};
        std::vector<size_t> heroes;
        for (double z = 40.0; z < zMax; z += m_heroSpacing * glm::mix(0.8f, 1.2f, unit(rng))) {
            Asteroid rock{};
            rock.size = glm::mix(6.0f, 13.0f, unit(rng));
            const glm::vec2 center(14.0f * std::sin(static_cast<float>(z) * 0.011f), 7.0f * std::sin(static_cast<float>(z) * 0.008f + 1.0f));
            const glm::vec2 offset((unit(rng) * 2.0f - 1.0f) * 16.0f, (unit(rng) * 2.0f - 1.0f) * 10.0f);
            rock.position = glm::vec3(center + offset, static_cast<float>(z));
            randomize(rock);
            const float back = (rock.size + m_passDistance) / lateral;
            waypoints.push_back(rock.position - lightDir * back);
            heroes.push_back(m_field.size());
            m_field.push_back(rock);
        }
        waypoints.push_back({glm::vec2(waypoints.back()), static_cast<float>(zMax + 300.0)});
        std::sort(waypoints.begin(), waypoints.end(), [](const glm::vec3& a, const glm::vec3& b) { return a.z < b.z; });
        m_path.set(std::move(waypoints));

        // 3: keep the ship (and the camera riding above the path) clear of every hero rock along the whole curve.
        for (size_t index : heroes) {
            Asteroid& rock = m_field[index];
            for (int iteration = 0; iteration < 4; ++iteration) {
                float nearest = 1e9f;
                glm::vec3 nearestPoint(0.0f);
                for (float dz = -rock.size - 40.0f; dz <= rock.size + 40.0f; dz += 1.0f) {
                    const glm::vec3 p = m_path.at(rock.position.z + dz);
                    for (const glm::vec3& q : {p, p + glm::vec3(0.0f, m_cameraHeight, 0.0f)}) {
                        const float d = glm::distance(q, rock.position);
                        if (d < nearest) {
                            nearest = d;
                            nearestPoint = q;
                        }
                    }
                }
                const float required = rock.size + 0.8f * m_passDistance;
                if (nearest >= required) break;
                rock.position += glm::normalize(rock.position - nearestPoint) * (required - nearest);
            }
        }

        // 4: the rest of the field. A third stays near the corridor for close passes; the rest spreads over a
        // wide disc, uniform over the annulus area so rocks never crowd the corridor wall.
        const int count = static_cast<int>((zMax - zMin) * 2.2 * density);
        m_field.reserve(m_field.size() + count);
        for (int i = 0; i < count; ++i) {
            Asteroid rock{};
            const double z = zMin + unit(rng) * (zMax - zMin);
            const float sizeRoll = unit(rng);
            const float sizeBlend = unit(rng);
            rock.size = sizeRoll < 0.6f   ? glm::mix(0.6f, 2.5f, sizeBlend)
                        : sizeRoll < 0.92f ? glm::mix(2.5f, 7.0f, sizeBlend)
                                           : glm::mix(9.0f, 26.0f, sizeBlend);
            const bool closePass = unit(rng) < 0.35f;
            const float minDistance = (m_clearance + rock.size) * 1.2f;
            const float outer = closePass ? 50.0f : (rock.size > 9.0f ? spread * 1.6f : spread);
            const float maxDistance = std::max(outer, minDistance + 1.0f);
            const float distance = std::sqrt(glm::mix(minDistance * minDistance, maxDistance * maxDistance, unit(rng)));
            const float angle = unit(rng) * 2.0f * std::numbers::pi_v<float>;
            rock.position = m_path.at(z) + glm::vec3(std::cos(angle) * distance, std::sin(angle) * distance, 0.0f);
            randomize(rock);
            m_field.push_back(rock);
        }
    }

    // Culls and sorts the field into per-mesh batches for the main pass and each shadow cascade, then packs
    // all instances (plus the ship) into one array.
    void buildInstances(double t, const Pose& ship, const glm::vec3& cameraPos, const Frustum& frustum,
                        const glm::mat4& lightView,
                        const std::array<glm::vec3, kCascades>& lightCenters,
                        const std::array<glm::vec4, kCascades>& cascades) {
        for (auto& bucket : m_mainBuckets) bucket.clear();
        for (auto& cascade : m_shadowBuckets)
            for (auto& bucket : cascade) bucket.clear();

        for (const Asteroid& rock : m_field) {
            const float bound = rock.size * 1.05f;
            const bool visible = frustum.intersectsSphere(rock.position, bound);
            const glm::vec3 light = glm::vec3(lightView * glm::vec4(rock.position, 1.0f));
            std::array<bool, kCascades> casts{};
            for (int c = 0; c < kCascades; ++c) {
                const float radius = cascades[c].w;
                const glm::vec3& center = lightCenters[c];
                casts[c] = std::abs(light.x - center.x) < radius + bound && std::abs(light.y - center.y) < radius + bound &&
                           -light.z > -center.z - radius - kCasterReach - bound && -light.z < -center.z + radius + bound;
            }
            if (!visible && !casts[0] && !casts[1]) continue;

            const float angle = rock.phase + rock.spin * static_cast<float>(t);
            glm::mat4 model = glm::translate(glm::mat4(1.0f), rock.position);
            model = glm::rotate(model, angle, rock.axis);
            model = glm::scale(model, glm::vec3(rock.size));
            const Instance instance{model, glm::vec4(rock.albedo, rock.size)};

            if (visible) {
                const int lod = glm::distance(cameraPos, rock.position) < 45.0f + rock.size * 6.0f ? 0 : 1;
                m_mainBuckets[rock.variant * kLods + lod].push_back(instance);
            }
            for (int c = 0; c < kCascades; ++c)
                if (casts[c]) m_shadowBuckets[c][rock.variant].push_back(instance);
        }

        m_instances.clear();
        m_mainBatches.clear();
        m_drawnRocks = 0;
        m_shadowRocks = 0;
        auto pack = [&](const std::vector<Instance>& bucket, const Mesh& mesh, std::vector<DrawBatch>& batches) {
            if (bucket.empty()) return;
            batches.push_back({&mesh, static_cast<GLuint>(m_instances.size()), static_cast<GLsizei>(bucket.size())});
            m_instances.insert(m_instances.end(), bucket.begin(), bucket.end());
        };
        for (int v = 0; v < kVariants; ++v)
            for (int lod = 0; lod < kLods; ++lod) {
                m_drawnRocks += static_cast<int>(m_mainBuckets[v * kLods + lod].size());
                pack(m_mainBuckets[v * kLods + lod], m_rocks[v][lod], m_mainBatches);
            }
        for (int c = 0; c < kCascades; ++c) {
            m_shadowBatches[c].clear();
            for (int v = 0; v < kVariants; ++v) {
                m_shadowRocks += static_cast<int>(m_shadowBuckets[c][v].size());
                pack(m_shadowBuckets[c][v], m_rocks[v][c == 0 ? 0 : 1], m_shadowBatches[c]);
            }
        }

        // The ship goes last. Mesh axes: +z nose, +y up, +x = cross(up, nose) (a proper rotation, no mirroring).
        glm::mat4 model(1.0f);
        model[0] = glm::vec4(glm::cross(ship.up, ship.forward), 0.0f);
        model[1] = glm::vec4(ship.up, 0.0f);
        model[2] = glm::vec4(ship.forward, 0.0f);
        model[3] = glm::vec4(ship.position, 1.0f);
        m_shipInstance = static_cast<GLuint>(m_instances.size());
        m_instances.push_back({model, glm::vec4(1.0f)});
    }

    Resources* m_resources = nullptr;
    ShaderProgram* m_skyProgram = nullptr;
    ShaderProgram* m_rockProgram = nullptr;
    ShaderProgram* m_shipProgram = nullptr;
    ShaderProgram* m_shadowProgram = nullptr;
    ShaderProgram* m_shaftsProgram = nullptr;
    ShaderProgram* m_compositeProgram = nullptr;

    std::array<std::array<Mesh, kLods>, kVariants> m_rocks;
    Mesh m_ship;
    std::unique_ptr<ShadowMap> m_shadowMap;
    StorageBuffer m_instanceBuffer;
    RenderTarget m_frame{GL_RGBA16F, true};
    RenderTarget m_shafts{GL_RGBA16F};

    std::vector<Asteroid> m_field;
    double m_fieldDuration = -1.0;
    float m_fieldSpeed = -1.0f;
    float m_fieldDensity = -1.0f;
    float m_fieldSpread = -1.0f;
    float m_fieldClearance = -1.0f;
    glm::vec3 m_fieldLight{0.0f};
    float m_fieldHeroSpacing = -1.0f;
    float m_fieldPassDistance = -1.0f;
    FlightPath m_path;

    std::array<std::vector<Instance>, kVariants * kLods> m_mainBuckets;
    std::array<std::array<std::vector<Instance>, kVariants>, kCascades> m_shadowBuckets;
    std::vector<Instance> m_instances;
    std::vector<DrawBatch> m_mainBatches;
    std::array<std::vector<DrawBatch>, kCascades> m_shadowBatches;
    GLuint m_shipInstance = 0;
    int m_drawnRocks = 0;
    int m_shadowRocks = 0;

    // Tunables (overlay sliders; timeline params override some of them).
    float m_speed = 28.0f;
    float m_density = 1.0f;
    float m_spread = 180.0f;
    float m_heroSpacing = 70.0f;   // distance between the rocks the path skims
    float m_passDistance = 5.0f;   // gap between a skimmed rock's surface and the ship's center line
    float m_clearance = 8.0f;  // corridor half-width kept free of rocks; covers the camera offset too
    float m_lag = 0.35f;
    float m_bank = 1.0f;
    float m_cameraHeight = 5.1f;
    float m_cameraBack = 5.0f;
    float m_focal = 1.2f;
    float m_starAzimuth = -32.0f;
    float m_starElevation = 18.0f;
    float m_starSize = 0.6f;
    float m_lightIntensity = 3.5f;
    float m_ambient = 0.04f;
    float m_softness = 2.0f;
    float m_rim = 0.15f;
    float m_bump = 1.0f;
    float m_shaftStrength = 1.2f;
    float m_shaftDecay = 0.975f;
    float m_flare = 1.0f;
    float m_engineGlow = 14.0f;
};

}  // namespace

REGISTER_SCENE("asteroids", AsteroidsScene);

}  // namespace demo

// Scene 7 — ringed giant: descending from above the rings, diving through the ring plane among icy chunks and
// looking back up at the planet banded by ring shadows. The planet and the ring sheet are raytraced (writing
// depth); within ~70 units of the camera the sheet fades into instanced ice chunks placed by the same radial
// density, so the dive passes real particles.
// Timeline params: sunaz / sunel (degrees), chunks (density multiplier).

#include "core/Resources.h"
#include "core/Shader.h"
#include "gfx/CpuNoise.h"
#include "gfx/Mesh.h"
#include "scene/Camera.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace demo {

namespace {

constexpr int kChunkVariants = 4;
constexpr float kPlanetRadius = 60.0f;
constexpr float kInner = 80.0f;
constexpr float kOuter = 150.0f;
constexpr float kNearFade = 70.0f;
constexpr float kCell = 2.5f;

struct Instance {
    glm::mat4 model;
    glm::vec4 color;
};

// CPU version of the ring density (shaders/scenes/rings/rings.glsl without the fine ringlet noise).
float ringDensity(float r) {
    if (r < kInner || r > kOuter) return 0.0f;
    const float x = (r - kInner) / (kOuter - kInner);
    const float base = glm::mix(0.3f, 0.95f, glm::smoothstep(0.1f, 0.45f, x)) * glm::mix(1.0f, 0.55f, glm::smoothstep(0.72f, 0.95f, x));
    const float wideGap = glm::smoothstep(2.0f, 3.5f, std::abs(r - (kInner + 0.69f * (kOuter - kInner))));
    const float narrowGap = glm::smoothstep(0.4f, 0.9f, std::abs(r - (kInner + 0.9f * (kOuter - kInner))));
    const float edges = glm::smoothstep(0.0f, 0.03f, x) * glm::smoothstep(1.0f, 0.96f, x);
    return base * wideGap * narrowGap * edges * 0.8f;
}

float distanceToSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 ab = b - a;
    const float t = std::clamp(glm::dot(p - a, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
    return glm::distance(p, a + ab * t);
}

class RingsScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_planet = &shaders.loadFullscreen("scene/rings/planet", "scenes/rings/planet.frag");
        m_ice = &shaders.load("scene/rings/ice", {{GL_VERTEX_SHADER, "common/mesh.vert"}, {GL_FRAGMENT_SHADER, "scenes/rings/ice.frag"}});
        for (int v = 0; v < kChunkVariants; ++v) m_chunks[v] = Mesh(makeAsteroid(5000u + static_cast<std::uint32_t>(v) * 31u, 2));
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const SceneParams& params = context.params;
        const float s = static_cast<float>(motion::smoothstep(0.0, 1.0, context.localTime / context.duration));

        // Straight descent through the dense ring (crossing the plane at r ~ 120), always facing the planet.
        const glm::vec3 from(80.0f, 40.0f, 160.0f), to(-40.0f, -30.0f, 90.0f);
        const Camera camera = Camera::lookAt(glm::mix(from, to, s), glm::vec3(0.0f, 5.0f, 0.0f), m_focal);

        const glm::vec2 resolution(context.size);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        const glm::mat4 projection = glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, 0.05f, 5000.0f);
        const glm::mat4 viewProj = projection * view;

        const float azimuth = glm::radians(params.get("sunaz", m_sunAzimuth));
        const float elevation = glm::radians(params.get("sunel", m_sunElevation));
        const glm::vec3 lightDir = glm::normalize(glm::vec3(std::sin(azimuth) * std::cos(elevation), std::sin(elevation),
                                                            std::cos(azimuth) * std::cos(elevation)));
        const glm::vec3 sunColor = glm::vec3(1.0f, 0.96f, 0.9f) * m_sunIntensity;

        m_frame.resize(context.size);
        m_frame.bind();
        m_frame.clear();

        // Raytraced planet, rings and sky; writes its own depth (test ALWAYS so gl_FragDepth always lands).
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        if (m_planet->valid()) {
            glDepthFunc(GL_ALWAYS);
            m_planet->use();
            camera.apply(*m_planet);
            setRingUniforms(*m_planet);
            m_planet->set("uResolution", resolution);
            m_planet->set("uTime", static_cast<float>(context.localTime));
            m_planet->set("uViewProj", viewProj);
            m_planet->set("uLightDir", lightDir);
            m_planet->set("uSunColor", sunColor);
            m_planet->set("uNearFade", kNearFade);
            m_resources->drawFullscreen();
        }
        glDepthFunc(GL_LESS);

        // Ice chunks around the camera while it is near the ring plane.
        buildChunks(camera.position, from, to, static_cast<float>(context.localTime), params.get("chunks", m_chunkDensity));
        if (m_ice->valid() && !m_instances.empty()) {
            m_instanceBuffer.upload(m_instances.data(), m_instances.size() * sizeof(Instance));
            m_instanceBuffer.bind(0);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            m_ice->use();
            setRingUniforms(*m_ice);
            m_ice->set("uViewProj", viewProj);
            m_ice->set("uCamPos", camera.position);
            m_ice->set("uLightDir", lightDir);
            m_ice->set("uSunColor", sunColor);
            for (int v = 0; v < kChunkVariants; ++v) m_chunks[v].draw(m_batchCounts[v], m_batchStarts[v]);
            glDisable(GL_CULL_FACE);
        }
        glDisable(GL_DEPTH_TEST);

        const glm::ivec2 size = m_frame.size();
        glBlitNamedFramebuffer(m_frame.fbo(), target.fbo(), 0, 0, size.x, size.y, 0, 0, size.x, size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        target.bind();
    }

    void imgui() override {
        ImGui::Text("%d ice chunks", static_cast<int>(m_instances.size()));
        ImGui::SliderFloat("sun azimuth", &m_sunAzimuth, -180.0f, 180.0f, "%.0f deg");
        ImGui::SliderFloat("sun elevation", &m_sunElevation, -60.0f, 60.0f, "%.0f deg");
        ImGui::SliderFloat("sun intensity", &m_sunIntensity, 0.5f, 6.0f);
        ImGui::SliderFloat("chunk density", &m_chunkDensity, 0.0f, 3.0f);
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
    }

private:
    void setRingUniforms(ShaderProgram& program) const {
        program.set("uPlanetRadius", kPlanetRadius);
        program.set("uInner", kInner);
        program.set("uOuter", kOuter);
    }

    // Chunks on a grid of cells around the camera (deterministic per cell), only where the ring is dense, in a
    // thin slab around the ring plane, and never on the camera's own path.
    void buildChunks(const glm::vec3& camera, const glm::vec3& pathFrom, const glm::vec3& pathTo, float time, float density) {
        std::array<std::vector<Instance>, kChunkVariants> buckets;
        if (std::abs(camera.y) < 45.0f && density > 0.0f) {
            const int reach = static_cast<int>(kNearFade / kCell);
            const int cx = static_cast<int>(std::floor(camera.x / kCell));
            const int cz = static_cast<int>(std::floor(camera.z / kCell));
            for (int iz = cz - reach; iz <= cz + reach; ++iz)
                for (int ix = cx - reach; ix <= cx + reach; ++ix) {
                    auto random = [&](int channel) { return cpunoise::hash(ix, channel, iz, 9157u); };
                    const glm::vec3 position((static_cast<float>(ix) + random(1)) * kCell, (random(2) - 0.5f) * 2.4f,
                                             (static_cast<float>(iz) + random(3)) * kCell);
                    if (glm::distance(glm::vec2(position.x, position.z), glm::vec2(camera.x, camera.z)) > kNearFade) continue;
                    if (random(4) > ringDensity(glm::length(glm::vec2(position.x, position.z))) * 1.1f * density) continue;
                    const float size = random(5) < 0.92f ? glm::mix(0.08f, 0.6f, random(6) * random(6)) : glm::mix(0.8f, 2.5f, random(6));
                    if (distanceToSegment(position, pathFrom, pathTo) < size + 1.5f) continue;

                    const glm::vec3 axis = glm::normalize(glm::vec3(random(7), random(8), random(9)) - 0.5f + glm::vec3(0.0f, 0.01f, 0.0f));
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
                    model = glm::rotate(model, random(10) * 6.283f + time * (random(11) - 0.5f), axis);
                    model = glm::scale(model, glm::vec3(size));
                    const glm::vec3 tint = glm::mix(glm::vec3(0.86f, 0.88f, 0.93f), glm::vec3(0.8f, 0.74f, 0.66f), random(12) * 0.6f);
                    buckets[std::min(static_cast<int>(random(13) * kChunkVariants), kChunkVariants - 1)].push_back({model, glm::vec4(tint, size)});
                }
        }
        m_instances.clear();
        for (int v = 0; v < kChunkVariants; ++v) {
            m_batchStarts[v] = static_cast<GLuint>(m_instances.size());
            m_batchCounts[v] = static_cast<GLsizei>(buckets[v].size());
            m_instances.insert(m_instances.end(), buckets[v].begin(), buckets[v].end());
        }
    }

    Resources* m_resources = nullptr;
    ShaderProgram* m_planet = nullptr;
    ShaderProgram* m_ice = nullptr;
    std::array<Mesh, kChunkVariants> m_chunks;
    StorageBuffer m_instanceBuffer;
    RenderTarget m_frame{GL_RGBA16F, true};
    std::vector<Instance> m_instances;
    std::array<GLuint, kChunkVariants> m_batchStarts{};
    std::array<GLsizei, kChunkVariants> m_batchCounts{};

    float m_sunAzimuth = 58.0f;
    float m_sunElevation = 20.0f;
    float m_sunIntensity = 2.2f;
    float m_chunkDensity = 1.0f;
    float m_focal = 1.1f;
};

}  // namespace

REGISTER_SCENE("rings", RingsScene);

}  // namespace demo

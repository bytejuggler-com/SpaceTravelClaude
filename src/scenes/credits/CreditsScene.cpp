// Scene 15 — end credits, demoscene style: a cluster of shiny vector balls morphing between shapes every 8 beats
// (sphere, cube lattice, torus knot, double helix, the word SPACE, a spiral) with a ripple running across the
// balls, pulsing on the beat; credits and greetings scroll up in the 8x8 font on the right.
// Timeline params: scroll (speed multiplier).

#include "core/Resources.h"
#include "core/Shader.h"
#include "gfx/Mesh.h"
#include "scene/Camera.h"
#include "scene/Motion.h"
#include "scene/SceneRegistry.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <string>
#include <vector>

namespace demo {

namespace {

constexpr int kBalls = 128;
constexpr double kBeatsPerShape = 8.0;
constexpr double kMorphBeats = 2.0;
constexpr float kBallRadius = 0.09f;
constexpr int kMaxLines = 48;  // must match scroller.frag

const std::vector<std::string> kCredits = {
    "SPACE TRAVEL",
    "",
    "A REAL-TIME DEMO",
    "C++20 AND OPENGL 4.6",
    "",
    "",
    "DIRECTION AND DESIGN",
    "BLACKLIZARD",
    "",
    "CODE",
    "CLAUDE",
    "",
    "MUSIC",
    "A TRACKER TUNE",
    "GENERATED IN CODE",
    "",
    "POWERED BY",
    "SDL3  LIBOPENMPT",
    "DEAR IMGUI",
    "GLM  GLAD  STB",
    "",
    "",
    "GREETINGS TO",
    "ALL AMIGA AND PC",
    "DEMOSCENERS",
    "COPPER BAR FANS",
    "WHOEVER WROTE",
    "A SINE SCROLLER",
    "",
    "",
    "THANKS FOR WATCHING!",
    "",
    "SEE YOU IN",
    "HYPERSPACE...",
};  // keep lines short (~20 characters): the column sits beside the vector balls

using Shape = std::array<glm::vec4, kBalls>;  // xyz position, w = scale (0 = hidden)

Shape sphereShape() {
    Shape shape{};
    const float golden = std::numbers::pi_v<float> * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < kBalls; ++i) {
        const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / kBalls;
        const float r = std::sqrt(1.0f - y * y);
        shape[i] = glm::vec4(std::cos(golden * i) * r, y, std::sin(golden * i) * r, 1.0f);
    }
    return shape;
}

Shape cubeShape() {
    Shape shape{};
    for (int i = 0; i < kBalls; ++i) {
        if (i >= 125) {
            shape[i] = glm::vec4(0.0f);  // 5^3 = 125 lattice points; the rest hide in the middle
            continue;
        }
        shape[i] = glm::vec4(glm::vec3(i % 5, (i / 5) % 5, i / 25) * 0.4f - 0.8f, 1.0f);
    }
    return shape;
}

Shape knotShape() {  // (2, 3) torus knot
    Shape shape{};
    for (int i = 0; i < kBalls; ++i) {
        const float a = 2.0f * std::numbers::pi_v<float> * i / kBalls;
        const float r = std::cos(3.0f * a) + 2.0f;
        shape[i] = glm::vec4(r * std::cos(2.0f * a) * 0.38f, r * std::sin(2.0f * a) * 0.38f, -std::sin(3.0f * a) * 0.38f, 1.0f);
    }
    return shape;
}

Shape helixShape() {
    Shape shape{};
    for (int i = 0; i < kBalls; ++i) {
        const int strand = i % 2;
        const float k = static_cast<float>(i / 2);
        const float a = k * 0.35f + strand * std::numbers::pi_v<float>;
        shape[i] = glm::vec4(std::cos(a) * 0.55f, k / (kBalls / 2) * 2.4f - 1.2f, std::sin(a) * 0.55f, 1.0f);
    }
    return shape;
}

Shape spiralShape() {
    Shape shape{};
    for (int i = 0; i < kBalls; ++i) {
        const int arm = i % 2;
        const float k = static_cast<float>(i / 2) / (kBalls / 2);
        const float a = k * 7.0f + arm * std::numbers::pi_v<float>;
        const float r = 0.15f + k * 1.2f;
        shape[i] = glm::vec4(std::cos(a) * r, std::sin(k * 9.0f) * 0.08f, std::sin(a) * r, 1.0f);
    }
    return shape;
}

Shape textShape(const BitmapFont& font, std::string_view text) {
    Shape shape{};
    const std::vector<glm::ivec2> pixels = font.pixels(text);
    const float width = static_cast<float>(text.size() * BitmapFont::kGlyphSize);
    const int step = std::max(1, static_cast<int>((pixels.size() + kBalls - 1) / kBalls));
    int ball = 0;
    for (size_t i = 0; i < pixels.size() && ball < kBalls; i += step, ++ball)
        shape[ball] = glm::vec4((static_cast<float>(pixels[i].x) - width * 0.5f) * 0.075f,
                                (3.5f - static_cast<float>(pixels[i].y)) * 0.075f, 0.0f, 1.0f);
    for (; ball < kBalls; ++ball) shape[ball] = glm::vec4(0.0f);
    return shape;
}

struct Instance {
    glm::mat4 model;
    glm::vec4 color;
};

class CreditsScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        ShaderLib& shaders = resources.shaders();
        m_background = &shaders.loadFullscreen("scene/credits/background", "scenes/credits/background.frag");
        m_ballProgram = &shaders.load("scene/credits/ball", {{GL_VERTEX_SHADER, "common/mesh.vert"}, {GL_FRAGMENT_SHADER, "scenes/credits/ball.frag"}});
        m_scroller = &shaders.loadFullscreen("scene/credits/scroller", "scenes/credits/scroller.frag");

        MeshData sphere = makeIcosphere(3);
        m_ball = Mesh(sphere);
        m_shapes = {sphereShape(), cubeShape(), knotShape(), helixShape(), textShape(resources.font(), "SPACE"), spiralShape()};
        m_textShape = 4;
        m_text.set(resources.font(), kCredits);
    }

    void render(const SceneContext& context, RenderTarget& target) override {
        const double t = context.localTime;
        const double beat = t * context.sync.bpm / 60.0;
        const int shapeCount = static_cast<int>(m_shapes.size());
        const int current = static_cast<int>(beat / kBeatsPerShape) % shapeCount;
        const int previous = (current + shapeCount - 1) % shapeCount;
        const double intoShape = std::fmod(beat, kBeatsPerShape);
        const bool firstShape = beat < kBeatsPerShape;

        // How much the cluster faces the camera: fully while it spells a word.
        const double morphAll = motion::smoothstep(0.0, kMorphBeats, intoShape);
        const double facing = current == m_textShape ? morphAll : (previous == m_textShape && !firstShape ? 1.0 - morphAll : 0.0);
        // The spin is left unwrapped (wrapping it mid-ramp snapped the cluster); instead the facing pose is moved
        // by whole turns, picked once at the start of the ramp (= the shape start), so the blend stays continuous.
        const double spin = t * 0.6;
        const double rampStart = (beat - intoShape) * 60.0 / context.sync.bpm;
        const double turns = std::round((rampStart * 0.6 - 0.25 * std::sin(rampStart * 0.5)) / (2.0 * std::numbers::pi));
        const double faceYaw = 0.25 * std::sin(t * 0.5) + turns * 2.0 * std::numbers::pi;
        const float yaw = static_cast<float>(glm::mix(spin, faceYaw, facing));
        const float pitch = static_cast<float>(glm::mix(0.35 * std::sin(t * 0.4), 0.0, facing));
        const glm::mat4 rotation = glm::rotate(glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1.0f, 0.0f, 0.0f)), yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 center(-1.6f, 0.15f, 0.0f);  // left of the credits column
        const float pulse = 1.0f + 0.3f * context.sync.beatPulse(6.0f);

        std::array<Instance, kBalls> instances{};
        for (int i = 0; i < kBalls; ++i) {
            // A ripple: each ball starts its morph a little after the previous one.
            const double m = firstShape ? 1.0 : motion::smoothstep(i * 0.006, kMorphBeats + i * 0.006, intoShape);
            const glm::vec4 a = m_shapes[previous][i], b = m_shapes[current][i];
            const glm::vec4 shape = glm::mix(firstShape ? b : a, b, static_cast<float>(m));
            const glm::vec3 position = center + glm::vec3(rotation * glm::vec4(glm::vec3(shape) * 1.3f, 1.0f));
            glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
            model = glm::scale(model, glm::vec3(kBallRadius * shape.w * pulse));
            const float hue = static_cast<float>(i) / kBalls + static_cast<float>(t) * 0.05f;
            const glm::vec3 tint = 0.55f + 0.45f * glm::cos(6.28318f * (glm::vec3(hue) + glm::vec3(0.0f, 0.33f, 0.67f)));
            instances[i] = {model, glm::vec4(tint, 1.0f)};
        }

        const Camera camera = Camera::lookAt(glm::vec3(0.0f, 0.3f, 6.0f), glm::vec3(0.0f, 0.1f, 0.0f), m_focal);
        const glm::vec2 resolution(context.size);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
        const glm::mat4 projection = glm::perspective(2.0f * std::atan(0.5f / camera.focal), resolution.x / resolution.y, 0.1f, 100.0f);

        m_frame.resize(context.size);
        m_frame.bind();
        m_frame.clear();
        if (m_background->valid()) {
            m_background->use();
            camera.apply(*m_background);
            m_background->set("uResolution", resolution);
            m_background->set("uTime", static_cast<float>(t));
            m_resources->drawFullscreen();
        }
        if (m_ballProgram->valid()) {
            m_instanceBuffer.upload(instances.data(), sizeof(instances));
            m_instanceBuffer.bind(0);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
            m_ballProgram->use();
            m_ballProgram->set("uViewProj", projection * view);
            m_ballProgram->set("uCamPos", camera.position);
            m_ballProgram->set("uShadowMap", 5);  // unused; keep sampler units distinct
            m_ball.draw(kBalls);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
        }
        if (m_scroller->valid()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            m_resources->font().bindAtlas(0);
            m_text.bind(1);
            m_scroller->use();
            m_scroller->set("uFont", 0);
            m_scroller->set("uText", 1);
            m_scroller->set("uResolution", resolution);
            m_scroller->set("uScroll", static_cast<float>(t * m_scrollSpeed * context.params.get("scroll", 1.0f) * resolution.y / 1080.0f));
            const int lines = std::min(static_cast<int>(kCredits.size()), kMaxLines);
            m_scroller->set("uLineCount", lines);
            for (int i = 0; i < lines; ++i) m_scroller->set(std::format("uLineLength[{}]", i).c_str(), m_text.length(i));
            m_scroller->set("uColumn", 0.72f);
            m_scroller->set("uTime", static_cast<float>(t));
            m_resources->drawFullscreen();
            glDisable(GL_BLEND);
        }

        const glm::ivec2 size = m_frame.size();
        glBlitNamedFramebuffer(m_frame.fbo(), target.fbo(), 0, 0, size.x, size.y, 0, 0, size.x, size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        target.bind();
    }

    void imgui() override {
        ImGui::SliderFloat("scroll speed", &m_scrollSpeed, 0.0f, 300.0f, "%.0f px/s");
        ImGui::SliderFloat("focal", &m_focal, 0.5f, 3.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_background = nullptr;
    ShaderProgram* m_ballProgram = nullptr;
    ShaderProgram* m_scroller = nullptr;
    Mesh m_ball;
    StorageBuffer m_instanceBuffer;
    TextTexture m_text;
    RenderTarget m_frame{GL_RGBA16F, true};
    std::vector<Shape> m_shapes;
    int m_textShape = 0;
    float m_scrollSpeed = 100.0f;
    float m_focal = 1.3f;
};

}  // namespace

REGISTER_SCENE("credits", CreditsScene);

}  // namespace demo

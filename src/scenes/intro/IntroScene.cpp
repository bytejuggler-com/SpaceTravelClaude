// Scene 1 — Amiga-style intro: copper bars, bouncing chrome logo, rainbow sine scroller with water reflection.
// Timeline params: scroll = scroller speed multiplier (default 1), kick = kick instrument number (default 1).

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/SceneRegistry.h"

#include <imgui.h>

namespace demo {

namespace {

constexpr const char* kLogo = "SPACE TRAVEL";
constexpr const char* kScrollText =
    "WELCOME ABOARD!     SPACE TRAVEL - A REAL-TIME DEMO IN C++ AND OPENGL 4.6 ...     "
    "FASTEN YOUR SEATBELTS, WE ARE ABOUT TO JUMP TO HYPERSPACE ...     "
    "COPPER BARS, SINE SCROLLERS AND 8X8 FONTS - JUST LIKE 1990 ...     "
    "GREETINGS TO ALL AMIGA AND PC DEMOSCENERS, PAST AND PRESENT!     ";

class IntroScene final : public Scene {
public:
    void init(Resources& resources) override {
        m_resources = &resources;
        m_shader = &resources.shaders().loadFullscreen("scene/intro", "scenes/intro/intro.frag");
        m_text.set(resources.font(), {kLogo, kScrollText});
    }

    void render(const SceneContext& context, RenderTarget& /*target*/) override {
        if (!m_shader->valid()) return;
        m_resources->font().bindAtlas(0);
        m_text.bind(1);

        m_shader->use();
        m_shader->set("uFont", 0);
        m_shader->set("uText", 1);
        m_shader->set("uTime", static_cast<float>(context.localTime));
        m_shader->set("uResolution", glm::vec2(context.size));
        m_shader->set("uKick", context.sync.instrumentPulse(static_cast<int>(context.params.get("kick", 1.0f)), 6.0f));
        m_shader->set("uBeatPulse", context.sync.beatPulse(5.0f));
        m_shader->set("uLogoLength", m_text.length(0));
        m_shader->set("uScrollLength", m_text.length(1));
        m_shader->set("uScrollSpeed", m_scrollSpeed * context.params.get("scroll", 1.0f));
        m_shader->set("uWobble", m_wobble);
        m_shader->set("uCopperSpeed", m_copperSpeed);
        m_resources->drawFullscreen();
    }

    void imgui() override {
        ImGui::SliderFloat("scroll speed", &m_scrollSpeed, 0.0f, 200.0f, "%.0f px/s");
        ImGui::SliderFloat("logo wobble", &m_wobble, 0.0f, 3.0f);
        ImGui::SliderFloat("copper speed", &m_copperSpeed, 0.0f, 4.0f);
    }

private:
    Resources* m_resources = nullptr;
    ShaderProgram* m_shader = nullptr;
    TextTexture m_text;
    float m_scrollSpeed = 70.0f;  // font pixels per second
    float m_wobble = 0.8f;
    float m_copperSpeed = 1.3f;
};

}  // namespace

REGISTER_SCENE("intro", IntroScene);

}  // namespace demo

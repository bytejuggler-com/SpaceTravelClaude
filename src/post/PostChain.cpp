#include "post/PostChain.h"

#include "core/Resources.h"
#include "core/Shader.h"
#include "scene/Scene.h"

#include <algorithm>

namespace demo {

namespace {

constexpr size_t kMaxBloomMips = 6;
constexpr int kMinBloomMipSize = 8;

}  // namespace

PostSettings PostSettings::withOverrides(const SceneParams& params) const {
    PostSettings s = *this;
    s.exposure = params.get("post.exposure", s.exposure);
    s.bloomIntensity = params.get("post.bloom", s.bloomIntensity);
    s.bloomThreshold = params.get("post.threshold", s.bloomThreshold);
    s.bloomKnee = params.get("post.knee", s.bloomKnee);
    s.bloomRadius = params.get("post.radius", s.bloomRadius);
    s.chromatic = params.get("post.chromatic", s.chromatic);
    s.vignette = params.get("post.vignette", s.vignette);
    s.grain = params.get("post.grain", s.grain);
    return s;
}

PostSettings PostSettings::lerp(const PostSettings& a, const PostSettings& b, float t) {
    auto mix = [t](float x, float y) { return x + (y - x) * t; };
    PostSettings s;
    s.exposure = mix(a.exposure, b.exposure);
    s.bloomIntensity = mix(a.bloomIntensity, b.bloomIntensity);
    s.bloomThreshold = mix(a.bloomThreshold, b.bloomThreshold);
    s.bloomKnee = mix(a.bloomKnee, b.bloomKnee);
    s.bloomRadius = mix(a.bloomRadius, b.bloomRadius);
    s.chromatic = mix(a.chromatic, b.chromatic);
    s.vignette = mix(a.vignette, b.vignette);
    s.grain = mix(a.grain, b.grain);
    return s;
}

PostChain::PostChain(Resources& resources) : m_resources(resources) {
    ShaderLib& shaders = resources.shaders();
    m_downsample = &shaders.loadFullscreen("post/bloom_down", "post/bloom_down.frag");
    m_upsample = &shaders.loadFullscreen("post/bloom_up", "post/bloom_up.frag");
    m_composite = &shaders.loadFullscreen("post/composite", "post/composite.frag");
}

void PostChain::apply(const RenderTarget& hdr, glm::ivec2 outputSize, const PostSettings& settings, float time) {
    renderBloom(hdr, settings);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outputSize.x, outputSize.y);

    if (!m_composite->valid()) {
        // Keep something on screen while the composite shader is broken.
        glBlitNamedFramebuffer(hdr.fbo(), 0, 0, 0, hdr.size().x, hdr.size().y, 0, 0, outputSize.x, outputSize.y,
                               GL_COLOR_BUFFER_BIT, GL_LINEAR);
        return;
    }

    hdr.bindTexture(0);
    m_mips.front().bindTexture(1);
    m_composite->use();
    m_composite->set("uScene", 0);
    m_composite->set("uBloom", 1);
    m_composite->set("uResolution", glm::vec2(outputSize));
    m_composite->set("uTime", time);
    m_composite->set("uExposure", settings.exposure);
    m_composite->set("uBloomIntensity", settings.bloomIntensity);
    m_composite->set("uChromatic", settings.chromatic);
    m_composite->set("uVignette", settings.vignette);
    m_composite->set("uGrain", settings.grain);
    m_resources.drawFullscreen();
}

void PostChain::renderBloom(const RenderTarget& hdr, const PostSettings& settings) {
    // Mip chain starting at half resolution.
    glm::ivec2 size = glm::max(hdr.size() / 2, glm::ivec2(1));
    size_t levels = 0;
    while (levels < kMaxBloomMips && (levels == 0 || std::min(size.x, size.y) >= kMinBloomMipSize)) {
        if (m_mips.size() <= levels) m_mips.emplace_back(GL_R11F_G11F_B10F);
        m_mips[levels].resize(size);
        size = glm::max(size / 2, glm::ivec2(1));
        ++levels;
    }
    m_mips.resize(levels);

    if (!m_downsample->valid() || !m_upsample->valid() || settings.bloomIntensity <= 0.0f) {
        m_mips.front().clear();
        return;
    }

    // Downsample, applying the soft threshold on the first pass.
    const float knee = std::max(settings.bloomThreshold * settings.bloomKnee, 1e-4f);
    m_downsample->use();
    m_downsample->set("uSource", 0);
    m_downsample->set("uThreshold", glm::vec4(settings.bloomThreshold, settings.bloomThreshold - knee, 2.0f * knee,
                                              0.25f / knee));
    const RenderTarget* source = &hdr;
    for (size_t i = 0; i < m_mips.size(); ++i) {
        m_mips[i].bind();
        m_downsample->set("uPrefilter", i == 0 ? 1 : 0);
        m_downsample->set("uSourceTexel", 1.0f / glm::vec2(source->size()));
        source->bindTexture(0);
        m_resources.drawFullscreen();
        source = &m_mips[i];
    }

    // Upsample back up, accumulating each level into the next larger one.
    m_upsample->use();
    m_upsample->set("uSource", 0);
    m_upsample->set("uRadius", settings.bloomRadius);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (size_t i = m_mips.size() - 1; i > 0; --i) {
        m_mips[i - 1].bind();
        m_upsample->set("uSourceTexel", 1.0f / glm::vec2(m_mips[i].size()));
        m_mips[i].bindTexture(0);
        m_resources.drawFullscreen();
    }
    glDisable(GL_BLEND);
}

}  // namespace demo

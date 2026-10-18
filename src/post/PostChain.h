#pragma once

#include "core/RenderTarget.h"

#include <glm/glm.hpp>

#include <vector>

namespace demo {

class Resources;
class SceneParams;
class ShaderProgram;

struct PostSettings {
    float exposure = 1.0f;
    float bloomIntensity = 0.5f;
    float bloomThreshold = 1.0f;  // HDR luminance where bloom starts
    float bloomKnee = 0.5f;       // soft-threshold width, as a fraction of the threshold
    float bloomRadius = 1.0f;     // upsample filter radius in texels
    float chromatic = 1.0f;       // radial chromatic aberration strength
    float vignette = 0.35f;       // corner darkening, 0..1
    float grain = 0.03f;          // film grain amplitude (also dithers away banding)

    // Timeline entries can override any field with "post.<name>=value", e.g. post.exposure=1.4.
    PostSettings withOverrides(const SceneParams& params) const;
    static PostSettings lerp(const PostSettings& a, const PostSettings& b, float t);
};

// HDR scene -> bloom (13-tap downsample / tent upsample mip chain) -> chromatic aberration ->
// ACES tonemap -> vignette, grain -> sRGB on the default framebuffer.
class PostChain {
public:
    explicit PostChain(Resources& resources);

    void apply(const RenderTarget& hdr, glm::ivec2 outputSize, const PostSettings& settings, float time);

private:
    void renderBloom(const RenderTarget& hdr, const PostSettings& settings);

    Resources& m_resources;
    ShaderProgram* m_downsample = nullptr;
    ShaderProgram* m_upsample = nullptr;
    ShaderProgram* m_composite = nullptr;
    std::vector<RenderTarget> m_mips;
};

}  // namespace demo

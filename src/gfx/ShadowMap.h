#pragma once

#include "core/Gl.h"

#include <glm/glm.hpp>

#include <vector>

namespace demo {

// Cascaded directional-light shadow map: a depth texture array (one layer per cascade) with hardware
// depth comparison enabled, sampled in GLSL as sampler2DArrayShadow (see shaders/common/lighting.glsl).
class ShadowMap {
public:
    ShadowMap(int size, int cascades);
    ~ShadowMap();
    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    void beginCascade(int cascade) const;  // binds and clears that layer as the depth target
    void bindTexture(GLuint unit) const { glBindTextureUnit(unit, m_texture); }
    int size() const { return m_size; }
    int cascades() const { return static_cast<int>(m_framebuffers.size()); }

private:
    int m_size;
    GLuint m_texture = 0;
    std::vector<GLuint> m_framebuffers;
};

// Light view-projection for one cascade of a directional light: an orthographic box around the sphere
// (center, radius), extended `casterReach` toward the light so occluders outside the sphere still cast,
// and snapped to whole texels so shadows do not shimmer as the camera moves.
glm::mat4 fitDirectionalShadow(const glm::vec3& lightDir, const glm::vec3& center, float radius, int mapSize,
                               float casterReach);

}  // namespace demo

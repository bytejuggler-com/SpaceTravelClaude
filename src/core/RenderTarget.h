#pragma once

#include "core/Gl.h"

#include <glm/glm.hpp>

namespace demo {

// Offscreen framebuffer with one color texture (linear filtering, clamp-to-edge) and an optional depth texture
// (32-bit float, nearest filtering, sampleable as a plain sampler2D).
// Storage is (re)allocated lazily by resize().
class RenderTarget {
public:
    explicit RenderTarget(GLenum colorFormat = GL_RGBA16F, bool withDepth = false)
        : m_format(colorFormat), m_hasDepth(withDepth) {}
    ~RenderTarget() { destroy(); }

    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;
    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    void resize(glm::ivec2 size);  // no-op when the size is unchanged
    void bind() const;             // binds the FBO and sets the viewport to cover it
    void clear(const glm::vec4& color = {0.0f, 0.0f, 0.0f, 1.0f}) const;
    void bindTexture(GLuint unit) const { glBindTextureUnit(unit, m_color); }
    void bindDepthTexture(GLuint unit) const { glBindTextureUnit(unit, m_depth); }

    GLuint texture() const { return m_color; }
    GLuint fbo() const { return m_fbo; }
    glm::ivec2 size() const { return m_size; }

private:
    void destroy();

    GLenum m_format;
    bool m_hasDepth;
    glm::ivec2 m_size{0};
    GLuint m_fbo = 0;
    GLuint m_color = 0;
    GLuint m_depth = 0;
};

}  // namespace demo

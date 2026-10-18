#include "core/RenderTarget.h"

#include "core/Log.h"

#include <utility>

namespace demo {

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : m_format(other.m_format),
      m_hasDepth(other.m_hasDepth),
      m_size(std::exchange(other.m_size, glm::ivec2(0))),
      m_fbo(std::exchange(other.m_fbo, 0u)),
      m_color(std::exchange(other.m_color, 0u)),
      m_depth(std::exchange(other.m_depth, 0u)) {}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
    if (this != &other) {
        destroy();
        m_format = other.m_format;
        m_hasDepth = other.m_hasDepth;
        m_size = std::exchange(other.m_size, glm::ivec2(0));
        m_fbo = std::exchange(other.m_fbo, 0u);
        m_color = std::exchange(other.m_color, 0u);
        m_depth = std::exchange(other.m_depth, 0u);
    }
    return *this;
}

void RenderTarget::resize(glm::ivec2 size) {
    size = glm::max(size, glm::ivec2(1));
    if (m_fbo && size == m_size) return;
    destroy();
    m_size = size;

    glCreateTextures(GL_TEXTURE_2D, 1, &m_color);
    glTextureStorage2D(m_color, 1, m_format, size.x, size.y);
    glTextureParameteri(m_color, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_color, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_color, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_color, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCreateFramebuffers(1, &m_fbo);
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT0, m_color, 0);

    if (m_hasDepth) {
        // A texture rather than a renderbuffer so later passes (light shafts, flare occlusion) can sample it.
        glCreateTextures(GL_TEXTURE_2D, 1, &m_depth);
        glTextureStorage2D(m_depth, 1, GL_DEPTH_COMPONENT32F, size.x, size.y);
        glTextureParameteri(m_depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(m_depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(m_depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(m_fbo, GL_DEPTH_ATTACHMENT, m_depth, 0);
    }

    const GLenum status = glCheckNamedFramebufferStatus(m_fbo, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        log::error("Framebuffer {}x{} incomplete (status 0x{:X})", size.x, size.y, status);
}

void RenderTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_size.x, m_size.y);
}

void RenderTarget::clear(const glm::vec4& color) const {
    glClearNamedFramebufferfv(m_fbo, GL_COLOR, 0, &color.x);
    if (m_hasDepth) {
        const float depth = 1.0f;
        glClearNamedFramebufferfv(m_fbo, GL_DEPTH, 0, &depth);
    }
}

void RenderTarget::destroy() {
    if (m_depth) glDeleteTextures(1, &m_depth);
    if (m_color) glDeleteTextures(1, &m_color);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    m_depth = m_color = m_fbo = 0;
    m_size = glm::ivec2(0);
}

}  // namespace demo

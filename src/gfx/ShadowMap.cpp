#include "gfx/ShadowMap.h"

#include "core/Log.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace demo {

glm::mat4 fitDirectionalShadow(const glm::vec3& lightDir, const glm::vec3& center, float radius, int mapSize,
                               float casterReach) {
    const glm::vec3 up = std::abs(lightDir.y) > 0.95f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), -lightDir, up);  // looking along the light's travel
    glm::vec3 c = glm::vec3(lightView * glm::vec4(center, 1.0f));
    const float texel = 2.0f * radius / static_cast<float>(mapSize);
    c.x = std::floor(c.x / texel) * texel;
    c.y = std::floor(c.y / texel) * texel;
    return glm::ortho(c.x - radius, c.x + radius, c.y - radius, c.y + radius, -c.z - radius - casterReach,
                      -c.z + radius) *
           lightView;
}

ShadowMap::ShadowMap(int size, int cascades) : m_size(size) {
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_texture);
    glTextureStorage3D(m_texture, 1, GL_DEPTH_COMPONENT32F, size, size, cascades);
    glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  // linear + compare = 2x2 hardware PCF
    glTextureParameteri(m_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(m_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // outside the map = lit
    glTextureParameterfv(m_texture, GL_TEXTURE_BORDER_COLOR, border);
    glTextureParameteri(m_texture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(m_texture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    m_framebuffers.resize(cascades);
    glCreateFramebuffers(cascades, m_framebuffers.data());
    for (int i = 0; i < cascades; ++i) {
        glNamedFramebufferTextureLayer(m_framebuffers[i], GL_DEPTH_ATTACHMENT, m_texture, 0, i);
        glNamedFramebufferDrawBuffer(m_framebuffers[i], GL_NONE);
        glNamedFramebufferReadBuffer(m_framebuffers[i], GL_NONE);
        if (glCheckNamedFramebufferStatus(m_framebuffers[i], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            log::error("Shadow map cascade {} framebuffer incomplete", i);
    }
}

ShadowMap::~ShadowMap() {
    glDeleteFramebuffers(static_cast<GLsizei>(m_framebuffers.size()), m_framebuffers.data());
    glDeleteTextures(1, &m_texture);
}

void ShadowMap::beginCascade(int cascade) const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[cascade]);
    glViewport(0, 0, m_size, m_size);
    const float depth = 1.0f;
    glClearNamedFramebufferfv(m_framebuffers[cascade], GL_DEPTH, 0, &depth);
}

}  // namespace demo

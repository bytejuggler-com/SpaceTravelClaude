#include "core/Resources.h"

#include "core/Log.h"
#include "core/Shader.h"

namespace demo {

namespace {
constexpr int kNoiseVolumeSize = 128;
}

Resources::Resources(ShaderLib& shaders) : m_shaders(shaders) { glCreateVertexArrays(1, &m_emptyVao); }

Resources::~Resources() {
    glDeleteVertexArrays(1, &m_emptyVao);
    if (m_noiseVolume) glDeleteTextures(1, &m_noiseVolume);
}

void Resources::drawFullscreen() const { drawProcedural(GL_TRIANGLES, 0, 3); }

void Resources::drawProcedural(GLenum mode, GLint first, GLsizei count) const {
    glBindVertexArray(m_emptyVao);
    glDrawArrays(mode, first, count);
}

GLuint Resources::noiseVolume() {
    if (m_noiseVolume) return m_noiseVolume;

    glCreateTextures(GL_TEXTURE_3D, 1, &m_noiseVolume);
    glTextureStorage3D(m_noiseVolume, 1, GL_RG8, kNoiseVolumeSize, kNoiseVolumeSize, kNoiseVolumeSize);
    glTextureParameteri(m_noiseVolume, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_noiseVolume, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    for (const GLenum wrap : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R})
        glTextureParameteri(m_noiseVolume, wrap, GL_REPEAT);

    ShaderProgram& generator = m_shaders.loadCompute("common/noise3d", "common/noise3d.comp");
    if (!generator.valid()) {
        log::error("Noise volume generator failed to build; volumes will be flat");
        return m_noiseVolume;
    }
    generator.use();
    generator.set("uSize", kNoiseVolumeSize);
    glBindImageTexture(0, m_noiseVolume, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG8);
    const GLuint groups = (kNoiseVolumeSize + 7) / 8;
    glDispatchCompute(groups, groups, groups);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    log::info("Noise volume {}^3 generated", kNoiseVolumeSize);
    return m_noiseVolume;
}

}  // namespace demo

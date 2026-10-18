#pragma once

#include "core/BitmapFont.h"
#include "core/Gl.h"

namespace demo {

class ShaderLib;

// Shared GPU helpers available to scenes and render passes.
class Resources {
public:
    explicit Resources(ShaderLib& shaders);
    ~Resources();
    Resources(const Resources&) = delete;
    Resources& operator=(const Resources&) = delete;

    ShaderLib& shaders() const { return m_shaders; }
    const BitmapFont& font() const { return m_font; }

    // Attribute-less fullscreen triangle; pair with shaders loaded via ShaderLib::loadFullscreen().
    void drawFullscreen() const;

    // Attribute-less draw: the vertex shader builds geometry from gl_VertexID
    // (e.g. 6 vertices per instanced particle quad).
    void drawProcedural(GLenum mode, GLint first, GLsizei count) const;

    // Tileable 128^3 RG8 noise volume for volumetric effects (R: value fbm, G: inverted Worley fbm), with
    // REPEAT wrapping and trilinear filtering. Generated on the GPU the first time it is requested.
    GLuint noiseVolume();
    void bindNoiseVolume(GLuint unit) { glBindTextureUnit(unit, noiseVolume()); }

private:
    ShaderLib& m_shaders;
    BitmapFont m_font;
    GLuint m_emptyVao = 0;  // core profile needs a bound VAO even for attribute-less draws
    GLuint m_noiseVolume = 0;
};

}  // namespace demo

#pragma once

#include "core/Gl.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace demo {

// Built-in chunky 8x8 font (uppercase, digits, common punctuation), stored as an R8 atlas with all glyphs
// side by side: glyph g occupies texels [g*8, g*8+8) x [0, 8), row 0 at the top. Glyph 0 is the space.
// Use from GLSL via shaders/common/font.glsl.
class BitmapFont {
public:
    static constexpr int kGlyphSize = 8;

    BitmapFont();
    ~BitmapFont();
    BitmapFont(const BitmapFont&) = delete;
    BitmapFont& operator=(const BitmapFont&) = delete;

    void bindAtlas(GLuint unit) const { glBindTextureUnit(unit, m_atlas); }
    std::uint8_t glyph(char c) const;  // lowercase maps to uppercase; unknown characters map to space
    std::vector<std::uint8_t> encode(std::string_view text) const;

    // Set pixels of `text` laid out on one line: x = column (8 per character), y = row from the top (0..7).
    std::vector<glm::ivec2> pixels(std::string_view text) const;

private:
    GLuint m_atlas = 0;
    std::array<std::uint8_t, 128> m_lookup{};
};

// Lines of text as glyph indices in an R8UI texture (one line per row), for usampler2D lookups.
class TextTexture {
public:
    TextTexture() = default;
    ~TextTexture();
    TextTexture(const TextTexture&) = delete;
    TextTexture& operator=(const TextTexture&) = delete;

    void set(const BitmapFont& font, const std::vector<std::string>& lines);
    void bind(GLuint unit) const { glBindTextureUnit(unit, m_texture); }
    int length(size_t line) const { return line < m_lengths.size() ? m_lengths[line] : 0; }

private:
    GLuint m_texture = 0;
    std::vector<int> m_lengths;
};

}  // namespace demo

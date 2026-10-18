#include "core/BitmapFont.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace demo {

namespace {

struct Glyph {
    char character;
    const char* bits;  // 8 rows of 8 columns, top row first; 'X' = set
};

// Two-pixel strokes, 7x7 design area, bottom row and right column left empty for spacing.
constexpr Glyph kGlyphs[] = {
    {' ', "........" "........" "........" "........" "........" "........" "........" "........"},
    {'!', "...XX..." "...XX..." "...XX..." "...XX..." "...XX..." "........" "...XX..." "........"},
    {'"', ".XX.XX.." ".XX.XX.." "........" "........" "........" "........" "........" "........"},
    {'\'', "...XX..." "...XX..." "..XX...." "........" "........" "........" "........" "........"},
    {'(', "....XX.." "...XX..." "..XX...." "..XX...." "..XX...." "...XX..." "....XX.." "........"},
    {')', "..XX...." "...XX..." "....XX.." "....XX.." "....XX.." "...XX..." "..XX...." "........"},
    {'*', "........" ".XX.XX.." "..XXX..." "XXXXXXX." "..XXX..." ".XX.XX.." "........" "........"},
    {'+', "........" "...XX..." "...XX..." ".XXXXXX." "...XX..." "...XX..." "........" "........"},
    {',', "........" "........" "........" "........" "........" "..XX...." "..XX...." ".XX....."},
    {'-', "........" "........" "........" ".XXXXX.." "........" "........" "........" "........"},
    {'.', "........" "........" "........" "........" "........" "..XX...." "..XX...." "........"},
    {'/', ".....XX." ".....XX." "....XX.." "...XX..." "..XX...." ".XX....." ".XX....." "........"},
    {'0', ".XXXXX.." "XX...XX." "XX..XXX." "XX.X.XX." "XXX..XX." "XX...XX." ".XXXXX.." "........"},
    {'1', "...XX..." "..XXX..." ".XXXX..." "...XX..." "...XX..." "...XX..." ".XXXXXX." "........"},
    {'2', ".XXXXX.." "XX...XX." ".....XX." "...XXX.." ".XXX...." "XX......" "XXXXXXX." "........"},
    {'3', ".XXXXX.." "XX...XX." ".....XX." "..XXXX.." ".....XX." "XX...XX." ".XXXXX.." "........"},
    {'4', "....XX.." "...XXX.." "..XXXX.." ".XX.XX.." "XXXXXXX." "....XX.." "....XX.." "........"},
    {'5', "XXXXXXX." "XX......" "XXXXXX.." ".....XX." ".....XX." "XX...XX." ".XXXXX.." "........"},
    {'6', "..XXXX.." ".XX....." "XX......" "XXXXXX.." "XX...XX." "XX...XX." ".XXXXX.." "........"},
    {'7', "XXXXXXX." ".....XX." "....XX.." "...XX..." "..XX...." "..XX...." "..XX...." "........"},
    {'8', ".XXXXX.." "XX...XX." "XX...XX." ".XXXXX.." "XX...XX." "XX...XX." ".XXXXX.." "........"},
    {'9', ".XXXXX.." "XX...XX." "XX...XX." ".XXXXXX." ".....XX." "....XX.." ".XXXX..." "........"},
    {':', "........" "..XX...." "..XX...." "........" "..XX...." "..XX...." "........" "........"},
    {'=', "........" "........" "XXXXXXX." "........" "XXXXXXX." "........" "........" "........"},
    {'?', ".XXXXX.." "XX...XX." "....XX.." "...XX..." "...XX..." "........" "...XX..." "........"},
    {'A', ".XXXXX.." "XX...XX." "XX...XX." "XXXXXXX." "XX...XX." "XX...XX." "XX...XX." "........"},
    {'B', "XXXXXX.." "XX...XX." "XX...XX." "XXXXXX.." "XX...XX." "XX...XX." "XXXXXX.." "........"},
    {'C', ".XXXXX.." "XX...XX." "XX......" "XX......" "XX......" "XX...XX." ".XXXXX.." "........"},
    {'D', "XXXXX..." "XX..XX.." "XX...XX." "XX...XX." "XX...XX." "XX..XX.." "XXXXX..." "........"},
    {'E', "XXXXXXX." "XX......" "XX......" "XXXXXX.." "XX......" "XX......" "XXXXXXX." "........"},
    {'F', "XXXXXXX." "XX......" "XX......" "XXXXXX.." "XX......" "XX......" "XX......" "........"},
    {'G', ".XXXXX.." "XX...XX." "XX......" "XX.XXXX." "XX...XX." "XX...XX." ".XXXXX.." "........"},
    {'H', "XX...XX." "XX...XX." "XX...XX." "XXXXXXX." "XX...XX." "XX...XX." "XX...XX." "........"},
    {'I', ".XXXXXX." "...XX..." "...XX..." "...XX..." "...XX..." "...XX..." ".XXXXXX." "........"},
    {'J', "....XXX." ".....XX." ".....XX." ".....XX." "XX...XX." "XX...XX." ".XXXXX.." "........"},
    {'K', "XX...XX." "XX..XX.." "XX.XX..." "XXXX...." "XX.XX..." "XX..XX.." "XX...XX." "........"},
    {'L', "XX......" "XX......" "XX......" "XX......" "XX......" "XX......" "XXXXXXX." "........"},
    {'M', "XX...XX." "XXX.XXX." "XXXXXXX." "XX.X.XX." "XX...XX." "XX...XX." "XX...XX." "........"},
    {'N', "XX...XX." "XXX..XX." "XXXX.XX." "XX.XXXX." "XX..XXX." "XX...XX." "XX...XX." "........"},
    {'O', ".XXXXX.." "XX...XX." "XX...XX." "XX...XX." "XX...XX." "XX...XX." ".XXXXX.." "........"},
    {'P', "XXXXXX.." "XX...XX." "XX...XX." "XXXXXX.." "XX......" "XX......" "XX......" "........"},
    {'Q', ".XXXXX.." "XX...XX." "XX...XX." "XX...XX." "XX.X.XX." "XX..XX.." ".XXX.XX." "........"},
    {'R', "XXXXXX.." "XX...XX." "XX...XX." "XXXXXX.." "XX.XX..." "XX..XX.." "XX...XX." "........"},
    {'S', ".XXXXX.." "XX...XX." "XX......" ".XXXXX.." ".....XX." "XX...XX." ".XXXXX.." "........"},
    {'T', "XXXXXX.." "..XX...." "..XX...." "..XX...." "..XX...." "..XX...." "..XX...." "........"},
    {'U', "XX...XX." "XX...XX." "XX...XX." "XX...XX." "XX...XX." "XX...XX." ".XXXXX.." "........"},
    {'V', "XX...XX." "XX...XX." "XX...XX." "XX...XX." ".XX.XX.." "..XXX..." "...X...." "........"},
    {'W', "XX...XX." "XX...XX." "XX...XX." "XX.X.XX." "XXXXXXX." "XXX.XXX." "XX...XX." "........"},
    {'X', "XX...XX." "XX...XX." ".XX.XX.." "..XXX..." ".XX.XX.." "XX...XX." "XX...XX." "........"},
    {'Y', "XX..XX.." "XX..XX.." "XX..XX.." ".XXXX..." "..XX...." "..XX...." "..XX...." "........"},
    {'Z', "XXXXXXX." ".....XX." "....XX.." "...XX..." "..XX...." ".XX....." "XXXXXXX." "........"},
};

constexpr int kGlyphCount = static_cast<int>(std::size(kGlyphs));

constexpr bool glyphsAreValid() {
    for (const Glyph& glyph : kGlyphs) {
        int length = 0;
        while (glyph.bits[length] != '\0') {
            if (glyph.bits[length] != 'X' && glyph.bits[length] != '.') return false;
            ++length;
        }
        if (length != BitmapFont::kGlyphSize * BitmapFont::kGlyphSize) return false;
    }
    return true;
}
static_assert(glyphsAreValid(), "every glyph must be exactly 8x8 of 'X' and '.'");
static_assert(kGlyphs[0].character == ' ', "glyph 0 must be the space");

}  // namespace

BitmapFont::BitmapFont() {
    constexpr int size = kGlyphSize;
    const int width = kGlyphCount * size;
    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * size, 0);
    for (int g = 0; g < kGlyphCount; ++g) {
        m_lookup[static_cast<unsigned char>(kGlyphs[g].character)] = static_cast<std::uint8_t>(g);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (kGlyphs[g].bits[y * size + x] == 'X') pixels[static_cast<size_t>(y) * width + g * size + x] = 255;
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &m_atlas);
    glTextureStorage2D(m_atlas, 1, GL_R8, width, size);
    glTextureParameteri(m_atlas, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(m_atlas, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_atlas, 0, 0, 0, width, size, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
}

BitmapFont::~BitmapFont() { glDeleteTextures(1, &m_atlas); }

std::uint8_t BitmapFont::glyph(char c) const {
    const int upper = std::toupper(static_cast<unsigned char>(c));
    return upper >= 0 && upper < 128 ? m_lookup[upper] : 0;
}

std::vector<std::uint8_t> BitmapFont::encode(std::string_view text) const {
    std::vector<std::uint8_t> glyphs;
    glyphs.reserve(text.size());
    for (char c : text) glyphs.push_back(glyph(c));
    return glyphs;
}

std::vector<glm::ivec2> BitmapFont::pixels(std::string_view text) const {
    std::vector<glm::ivec2> result;
    for (size_t i = 0; i < text.size(); ++i) {
        const Glyph& g = kGlyphs[glyph(text[i])];
        for (int y = 0; y < kGlyphSize; ++y)
            for (int x = 0; x < kGlyphSize; ++x)
                if (g.bits[y * kGlyphSize + x] == 'X') result.emplace_back(static_cast<int>(i) * kGlyphSize + x, y);
    }
    return result;
}

TextTexture::~TextTexture() {
    if (m_texture) glDeleteTextures(1, &m_texture);
}

void TextTexture::set(const BitmapFont& font, const std::vector<std::string>& lines) {
    if (m_texture) glDeleteTextures(1, &m_texture);
    m_lengths.clear();

    size_t width = 1;
    for (const std::string& line : lines) width = std::max(width, line.size());
    const int height = std::max(1, static_cast<int>(lines.size()));

    std::vector<std::uint8_t> glyphs(width * height, 0);
    for (size_t row = 0; row < lines.size(); ++row) {
        const std::vector<std::uint8_t> encoded = font.encode(lines[row]);
        std::copy(encoded.begin(), encoded.end(), glyphs.begin() + static_cast<std::ptrdiff_t>(row * width));
        m_lengths.push_back(static_cast<int>(encoded.size()));
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &m_texture);
    glTextureStorage2D(m_texture, 1, GL_R8UI, static_cast<GLsizei>(width), height);
    glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // integer textures must not filter
    glTextureParameteri(m_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(m_texture, 0, 0, 0, static_cast<GLsizei>(width), height, GL_RED_INTEGER, GL_UNSIGNED_BYTE,
                        glyphs.data());
}

}  // namespace demo

// Built-in 8x8 bitmap font (see src/core/BitmapFont.h). Include with: #include "common/font.glsl"
// The scene binds the glyph atlas to uFont and a TextTexture (one line of glyph indices per row) to uText.

uniform sampler2D uFont;
uniform usampler2D uText;

// Coverage (0 or 1) of text pixel p on `line`: p.x runs along the whole line (8 pixels per character),
// p.y is 0..7 from the top of the glyphs. Outside the text returns 0.
float textPixel(ivec2 p, int line, int length) {
    if (p.x < 0 || p.y < 0 || p.y >= 8 || p.x >= length * 8) return 0.0;
    uint glyph = texelFetch(uText, ivec2(p.x / 8, line), 0).r;
    return texelFetch(uFont, ivec2(int(glyph) * 8 + p.x % 8, p.y), 0).r;
}

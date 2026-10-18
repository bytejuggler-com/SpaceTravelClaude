#version 460 core

#include "common/color.glsl"
#include "common/font.glsl"

// Vertical end-credits scroller in the 8x8 font: lines centered in a column on the right, rising slowly,
// fading in at the bottom and out at the top. Blended over the frame (alpha = text coverage).

in vec2 vUV;
out vec4 fragColor;

const int kMaxLines = 48;

uniform vec2 uResolution;
uniform float uScroll;          // pixels scrolled so far
uniform int uLineCount;
uniform int uLineLength[kMaxLines];
uniform float uColumn;          // column center, fraction of the width
uniform float uTime;

void main() {
    float pix = max(1.0, floor(uResolution.y / 220.0));  // screen pixels per font pixel
    float lineHeight = 12.0 * pix;
    float fromTop = uResolution.y - gl_FragCoord.y;
    float scrolled = fromTop - uResolution.y + uScroll;   // text starts below the screen and rises
    int line = int(floor(scrolled / lineHeight));
    if (scrolled < 0.0 || line >= uLineCount) discard;

    float row = (scrolled - float(line) * lineHeight) / pix;
    int lineLength = uLineLength[line];
    float left = uColumn * uResolution.x - float(lineLength * 8) * pix * 0.5;
    ivec2 p = ivec2(floor((gl_FragCoord.x - left) / pix), int(floor(row)));
    float on = textPixel(p, line, lineLength);
    if (on < 0.5) discard;

    vec2 f = fract(vec2((gl_FragCoord.x - left) / pix, row));
    float tile = step(0.12, f.x) * step(0.12, f.y);
    vec3 tint = palette(float(line) * 0.07 + uTime * 0.05, vec3(0.7), vec3(0.3), vec3(1.0), vec3(0.0, 0.33, 0.67));
    float fade = smoothstep(0.0, 0.12, gl_FragCoord.y / uResolution.y) * smoothstep(1.0, 0.85, gl_FragCoord.y / uResolution.y);
    fragColor = vec4(tint * 1.6 * mix(0.7, 1.0, tile), fade * mix(0.6, 1.0, tile));
}

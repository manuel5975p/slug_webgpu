/*
 * slug_math.h — LaTeX-like math typesetting for Slug text rendering
 *
 * Builds on slug.h to provide superscripts, subscripts, and stacked
 * fractions for plot axis labels and simple mathematical expressions.
 *
 * Markup syntax:
 *   ^{expr}        superscript           x^{2+n}
 *   ^c             single-char super     x^2
 *   _{expr}        subscript             a_{max}
 *   _c             single-char sub       a_n
 *   \frac{n}{d}    stacked fraction      \frac{1}{2}
 *   \\  \^  \_     literal escapes
 *
 * Nesting is supported: x^{2^{n}}, \frac{x^{2}+1}{x-1}
 *
 * Usage:
 *   SlugTextData sd = slug_prepare_math_text(&font, 1, "E = mc^{2}", 48.0f);
 *   // ... upload sd to GPU, render with Slug shaders ...
 *   slug_free_text_data(&sd);
 */
#ifndef SLUG_MATH_H
#define SLUG_MATH_H

#include "slug.h"

/*
 * Prepare GPU-ready text data with math markup.
 *
 *   fonts     — array of stbtt_fontinfo (tried in order for glyph fallback)
 *   fontCount — number of fonts (1 for single font)
 *   markup    — UTF-8 string with LaTeX-like markup
 *   fontSize  — base pixel height for normal text
 *
 * Returns SlugTextData with totalAdvance in PIXELS (not font units).
 * Caller must free with slug_free_text_data().
 */
SlugTextData slug_prepare_math_text(const stbtt_fontinfo *fonts, int fontCount,
                                    const char *markup, float fontSize);

#endif /* SLUG_MATH_H */

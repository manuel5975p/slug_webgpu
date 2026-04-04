/*
 * slug.h — GPU-accelerated text rendering via the Slug algorithm
 *
 * Slug renders text by evaluating quadratic Bezier curves directly in the
 * fragment shader, producing resolution-independent text at any size or zoom.
 * Fonts are parsed with stb_truetype; cubic curves are split into quadratics.
 *
 * Usage:
 *   1. Load a TTF font with stbtt_InitFont().
 *   2. Call slug_prepare_text() to get GPU-ready vertex/index/texture data.
 *   3. Upload the data to WebGPU buffers and textures.
 *   4. Render with the Slug vertex + pixel shaders.
 *   5. Call slug_free_text_data() when done.
 *
 * For math typesetting (superscripts, subscripts, fractions), see slug_math.h.
 *
 * Vertex layout (80 bytes, 5 x vec4f per vertex):
 *   attr 0: position.xy, normal.xy        (dilation direction)
 *   attr 1: glyphCoord.xy, glyphLoc, bandMaxes  (packed u32-as-f32)
 *   attr 2: jacobian (2x2 matrix)         (pixel-to-glyph-space transform)
 *   attr 3: bandScale.xy, bandOffset.xy   (band lookup transform)
 *   attr 4: color.rgba
 *
 * Texture formats:
 *   Curve texture: RGBA32Float, width = SLUG_TEX_WIDTH
 *   Band texture:  RGBA32Uint,  width = SLUG_TEX_WIDTH
 */
#ifndef SLUG_H
#define SLUG_H

#include <stdint.h>
#include <stb_truetype.h>

/* Internal constants — needed by callers to create GPU textures of the right width. */
#define SLUG_TEX_WIDTH  4096
#define SLUG_BAND_COUNT 8

/* ------------------------------------------------------------------ */
/*  Public types                                                      */
/* ------------------------------------------------------------------ */

/*
 * GPU-ready data for rendering a block of text.
 * Contains vertex/index buffers and curve/band textures.
 * Free with slug_free_text_data() when done.
 */
typedef struct {
    float    *vertices;       /* 20 floats per vertex (5 x vec4f = 80 bytes)       */
    uint32_t *indices;        /* 6 indices per glyph quad (2 triangles)             */
    int       vertexCount;
    int       indexCount;

    float    *curveTexData;   /* RGBA32Float pixel data, width = SLUG_TEX_WIDTH     */
    uint32_t *bandTexData;    /* RGBA32Uint  pixel data, width = SLUG_TEX_WIDTH     */
    int       curveTexHeight; /* height of curve texture (rows)                     */
    int       bandTexHeight;  /* height of band texture (rows)                      */

    float     totalAdvance;   /* total horizontal extent                            *
                               *   slug_prepare_text: in font units (× scale → px) *
                               *   slug_prepare_runs/math: in pixels directly       */
} SlugTextData;

/*
 * A positioned text run — used by slug_prepare_runs() to compose
 * multi-run layouts (e.g., math typesetting with mixed sizes).
 */
typedef struct {
    const stbtt_fontinfo *font;
    const char *text;       /* UTF-8 string for this run (not owned)   */
    float fontSize;         /* target pixel height                      */
    float offsetX;          /* horizontal offset in pixels              */
    float offsetY;          /* vertical offset in pixels (positive=up)  */
} SlugTextRun;

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/*
 * Prepare GPU-ready text data for a single line of text.
 *
 *   font      — initialized stbtt_fontinfo (from stbtt_InitFont)
 *   text      — UTF-8 encoded string
 *   fontSize  — desired pixel height (e.g., 48.0f)
 *
 * Returns a SlugTextData with all buffers allocated.
 * totalAdvance is in font units — multiply by
 * stbtt_ScaleForPixelHeight(font, fontSize) to get pixels.
 *
 * Caller must free with slug_free_text_data().
 */
SlugTextData slug_prepare_text(const stbtt_fontinfo *font,
                               const char *text, float fontSize);

/*
 * Prepare GPU-ready text data from multiple positioned runs.
 *
 * Each run can have a different font, font size, and position.
 * Glyphs are deduplicated across runs and packed into shared textures.
 *
 * totalAdvance is in pixels (the rightmost extent across all runs).
 *
 * Caller must free with slug_free_text_data().
 */
SlugTextData slug_prepare_runs(const SlugTextRun *runs, int runCount);

/*
 * Free all memory owned by a SlugTextData.
 * Safe to call on a zero-initialized struct (no-op).
 */
void slug_free_text_data(SlugTextData *data);

#endif /* SLUG_H */

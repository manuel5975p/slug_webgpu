#include "slug_math.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Typographic constants (TeX-style)                                 */
/* ------------------------------------------------------------------ */

#define SUP_SCALE   0.7f   /* superscript font size ratio */
#define SUP_RAISE   0.45f  /* raise as fraction of ascent */
#define SUB_SCALE   0.7f   /* subscript font size ratio */
#define SUB_DROP    0.25f  /* drop as fraction of ascent */
#define FRAC_SCALE  0.65f  /* fraction num/den font size ratio */
#define FRAC_BAR_Y  0.35f  /* math axis as fraction of ascent */
#define FRAC_GAP    0.12f  /* gap above/below fraction bar as fraction of ascent */
#define FRAC_BAR_SZ 0.4f   /* fraction bar font size relative to fracSz */
#define FRAC_PAD    0.15f  /* horizontal padding on each side of bar, as fraction of ascent */

/* ------------------------------------------------------------------ */
/*  UTF-8 decoder (matches slug.c)                                    */
/* ------------------------------------------------------------------ */

static int utf8_next(const char *s, int len, int *pos)
{
    unsigned char c = (unsigned char)s[*pos];
    int cp, n;
    if (c < 0x80)                              { cp = c;                                     n = 1; }
    else if ((c & 0xE0) == 0xC0 && *pos+1<len) { cp = (c&0x1F)<<6  | (s[*pos+1]&0x3F);      n = 2; }
    else if ((c & 0xF0) == 0xE0 && *pos+2<len) { cp = (c&0x0F)<<12 | (s[*pos+1]&0x3F)<<6
                                                      | (s[*pos+2]&0x3F);                     n = 3; }
    else if ((c & 0xF8) == 0xF0 && *pos+3<len) { cp = (c&0x07)<<18 | (s[*pos+1]&0x3F)<<12
                                                      | (s[*pos+2]&0x3F)<<6 | (s[*pos+3]&0x3F); n = 4; }
    else                                       { cp = 0xFFFD;                                n = 1; }
    *pos += n;
    return cp;
}

/* Return byte length of a single UTF-8 character starting at s[0]. */
static int utf8_char_len(const char *s, int remaining)
{
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80)                              return 1;
    if ((c & 0xE0) == 0xC0 && remaining >= 2)  return 2;
    if ((c & 0xF0) == 0xE0 && remaining >= 3)  return 3;
    if ((c & 0xF8) == 0xF0 && remaining >= 4)  return 4;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  measure_text: width in pixels using stbtt metrics                  */
/* ------------------------------------------------------------------ */

static float measure_text(const stbtt_fontinfo *font, const char *text, float fontSize)
{
    if (!text || !text[0]) return 0.0f;

    float scale = stbtt_ScaleForPixelHeight(font, fontSize);
    int len = (int)strlen(text);
    float width = 0.0f;
    int prevGlyph = 0;

    for (int pos = 0; pos < len; ) {
        int cp = utf8_next(text, len, &pos);
        int glyph = stbtt_FindGlyphIndex(font, cp);

        if (prevGlyph && glyph) {
            int kern = stbtt_GetGlyphKernAdvance(font, prevGlyph, glyph);
            width += (float)kern * scale;
        }

        int advW, lsb;
        stbtt_GetGlyphHMetrics(font, glyph, &advW, &lsb);
        width += (float)advW * scale;

        prevGlyph = glyph;
    }
    return width;
}

/* ------------------------------------------------------------------ */
/*  Dynamic run list                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    SlugTextRun *runs;
    int count;
    int cap;
} RunList;

static void runlist_init(RunList *rl)
{
    rl->count = 0;
    rl->cap   = 16;
    rl->runs  = (SlugTextRun *)malloc(rl->cap * sizeof(SlugTextRun));
}

static void runlist_push(RunList *rl, SlugTextRun run)
{
    if (rl->count >= rl->cap) {
        rl->cap *= 2;
        rl->runs = (SlugTextRun *)realloc(rl->runs, rl->cap * sizeof(SlugTextRun));
    }
    rl->runs[rl->count++] = run;
}

/* ------------------------------------------------------------------ */
/*  String buffer                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    int len;
    int cap;
} StrBuf;

static void strbuf_init(StrBuf *sb)
{
    sb->len  = 0;
    sb->cap  = 64;
    sb->data = (char *)malloc(sb->cap);
    sb->data[0] = '\0';
}

static void strbuf_append(StrBuf *sb, const char *s, int n)
{
    if (n <= 0) return;
    while (sb->len + n + 1 > sb->cap) {
        sb->cap *= 2;
        sb->data = (char *)realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void strbuf_clear(StrBuf *sb)
{
    sb->len = 0;
    sb->data[0] = '\0';
}

static void strbuf_free(StrBuf *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

/* ------------------------------------------------------------------ */
/*  Allocation tracker (for malloc'd run text strings)                 */
/* ------------------------------------------------------------------ */

typedef struct {
    char **ptrs;
    int count;
    int cap;
} AllocTracker;

static void alloctrk_init(AllocTracker *at)
{
    at->count = 0;
    at->cap   = 32;
    at->ptrs  = (char **)malloc(at->cap * sizeof(char *));
}

static char *alloctrk_dup(AllocTracker *at, const char *s)
{
    if (at->count >= at->cap) {
        at->cap *= 2;
        at->ptrs = (char **)realloc(at->ptrs, at->cap * sizeof(char *));
    }
    char *dup = (char *)malloc(strlen(s) + 1);
    strcpy(dup, s);
    at->ptrs[at->count++] = dup;
    return dup;
}

static void alloctrk_free_all(AllocTracker *at)
{
    for (int i = 0; i < at->count; i++)
        free(at->ptrs[i]);
    free(at->ptrs);
    at->ptrs  = NULL;
    at->count = at->cap = 0;
}

/* ------------------------------------------------------------------ */
/*  Parser state                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    const char          *src;
    int                  len;
    int                  pos;
    const stbtt_fontinfo *font;     /* primary font */
    const stbtt_fontinfo *fonts;    /* font array for fallback */
    int                  fontCount;
    RunList             *rl;
    AllocTracker        *at;
    StrBuf               buf;       /* accumulated plain text */
} Parser;

/* Forward declaration */
static float parse_expr(Parser *p, float fontSize, float baseX, float baseY);

/* ------------------------------------------------------------------ */
/*  parse_group: read brace-delimited or single UTF-8 char             */
/* ------------------------------------------------------------------ */

static char *parse_group(Parser *p)
{
    if (p->pos >= p->len) {
        char *empty = (char *)malloc(1);
        empty[0] = '\0';
        return empty;
    }

    if (p->src[p->pos] == '{') {
        p->pos++; /* skip '{' */
        int depth = 1;
        int start = p->pos;
        while (p->pos < p->len && depth > 0) {
            if (p->src[p->pos] == '{') depth++;
            else if (p->src[p->pos] == '}') depth--;
            if (depth > 0) p->pos++;
        }
        int end = p->pos;
        if (p->pos < p->len) p->pos++; /* skip closing '}' */
        int n = end - start;
        char *s = (char *)malloc(n + 1);
        memcpy(s, p->src + start, n);
        s[n] = '\0';
        return s;
    } else {
        /* single UTF-8 character */
        int n = utf8_char_len(p->src + p->pos, p->len - p->pos);
        char *s = (char *)malloc(n + 1);
        memcpy(s, p->src + p->pos, n);
        s[n] = '\0';
        p->pos += n;
        return s;
    }
}

/* ------------------------------------------------------------------ */
/*  Flush accumulated plain text as a run                              */
/* ------------------------------------------------------------------ */

static float flush_plain(Parser *p, float fontSize, float curX, float baseY)
{
    if (p->buf.len == 0) return 0.0f;

    char *text = alloctrk_dup(p->at, p->buf.data);
    float w = measure_text(p->font, text, fontSize);

    SlugTextRun run = {
        .font     = p->font,
        .text     = text,
        .fontSize = fontSize,
        .offsetX  = curX,
        .offsetY  = baseY,
    };
    runlist_push(p->rl, run);

    strbuf_clear(&p->buf);
    return w;
}

/* ------------------------------------------------------------------ */
/*  Check for \frac command                                            */
/* ------------------------------------------------------------------ */

static int match_cmd(const char *s, int pos, int len, const char *cmd)
{
    int cl = (int)strlen(cmd);
    if (pos + cl > len) return 0;
    return memcmp(s + pos, cmd, cl) == 0;
}

/* ------------------------------------------------------------------ */
/*  Recursive expression parser                                        */
/* ------------------------------------------------------------------ */

static float parse_expr(Parser *p, float fontSize, float baseX, float baseY)
{
    float curX = baseX;

    while (p->pos < p->len) {
        char ch = p->src[p->pos];

        /* End of brace group */
        if (ch == '}') break;

        /* Superscript */
        if (ch == '^') {
            curX += flush_plain(p, fontSize, curX, baseY);
            p->pos++; /* skip '^' */

            float scale = stbtt_ScaleForPixelHeight(p->font, fontSize);
            int asc, desc, lg;
            stbtt_GetFontVMetrics(p->font, &asc, &desc, &lg);
            float ascPx = (float)asc * scale;

            float supSize = fontSize * SUP_SCALE;
            float supY    = baseY + ascPx * SUP_RAISE;

            /* Parse group into a sub-expression */
            char *grp = parse_group(p);
            /* Create a sub-parser to handle the group content */
            Parser sub = *p;
            sub.src = grp;
            sub.len = (int)strlen(grp);
            sub.pos = 0;
            strbuf_init(&sub.buf);

            float w = parse_expr(&sub, supSize, curX, supY);
            /* flush any remaining text in sub */
            w += flush_plain(&sub, supSize, curX + w, supY);

            strbuf_free(&sub.buf);
            free(grp);
            curX += w;
            continue;
        }

        /* Subscript */
        if (ch == '_') {
            curX += flush_plain(p, fontSize, curX, baseY);
            p->pos++; /* skip '_' */

            float scale = stbtt_ScaleForPixelHeight(p->font, fontSize);
            int asc, desc, lg;
            stbtt_GetFontVMetrics(p->font, &asc, &desc, &lg);
            float ascPx = (float)asc * scale;

            float subSize = fontSize * SUB_SCALE;
            float subY    = baseY - ascPx * SUB_DROP;

            char *grp = parse_group(p);
            Parser sub = *p;
            sub.src = grp;
            sub.len = (int)strlen(grp);
            sub.pos = 0;
            strbuf_init(&sub.buf);

            float w = parse_expr(&sub, subSize, curX, subY);
            w += flush_plain(&sub, subSize, curX + w, subY);

            strbuf_free(&sub.buf);
            free(grp);
            curX += w;
            continue;
        }

        /* Backslash escapes and commands */
        if (ch == '\\') {
            curX += flush_plain(p, fontSize, curX, baseY);
            p->pos++; /* skip '\\' */

            if (p->pos >= p->len) break;

            /* Literal escapes: \\  \^  \_ */
            if (p->src[p->pos] == '\\' || p->src[p->pos] == '^' || p->src[p->pos] == '_') {
                strbuf_append(&p->buf, p->src + p->pos, 1);
                p->pos++;
                continue;
            }

            /* \frac{num}{den} */
            if (match_cmd(p->src, p->pos, p->len, "frac")) {
                p->pos += 4; /* skip "frac" */

                float scale = stbtt_ScaleForPixelHeight(p->font, fontSize);
                int asc, desc, lg;
                stbtt_GetFontVMetrics(p->font, &asc, &desc, &lg);
                float ascPx  = (float)asc * scale;
                float barY   = baseY + ascPx * FRAC_BAR_Y;
                float gapPx  = ascPx * FRAC_GAP;
                float fracSz = fontSize * FRAC_SCALE;

                /* Fraction-sized ascent (for positioning denominator) */
                float fracScale = stbtt_ScaleForPixelHeight(p->font, fracSz);
                float fracAsc   = (float)asc * fracScale;

                /* Parse numerator and denominator groups */
                char *numStr = parse_group(p);
                char *denStr = parse_group(p);

                /* First pass: parse into temporary run lists to measure widths */
                RunList numRL, denRL;
                runlist_init(&numRL);
                runlist_init(&denRL);

                /* Parse numerator */
                Parser numP = *p;
                numP.src = numStr;
                numP.len = (int)strlen(numStr);
                numP.pos = 0;
                numP.rl  = &numRL;
                strbuf_init(&numP.buf);
                float numW = parse_expr(&numP, fracSz, 0.0f, 0.0f);
                strbuf_free(&numP.buf);

                /* Parse denominator */
                Parser denP = *p;
                denP.src = denStr;
                denP.len = (int)strlen(denStr);
                denP.pos = 0;
                denP.rl  = &denRL;
                strbuf_init(&denP.buf);
                float denW = parse_expr(&denP, fracSz, 0.0f, 0.0f);
                strbuf_free(&denP.buf);

                float padPx = ascPx * FRAC_PAD;
                float maxW = (numW > denW ? numW : denW) + 2.0f * padPx;

                /* Numerator: baseline sits above bar + gap.
                   The descender of the numerator text should clear the bar. */
                float numOffX = curX + (maxW - numW) * 0.5f;
                float numY = barY + gapPx;

                /* Denominator: top of text (ascent) sits below bar - gap.
                   offsetY is the baseline, so we need baseline = barY - gap - fracAsc */
                float denOffX = curX + (maxW - denW) * 0.5f;
                float denY = barY - gapPx - fracAsc;

                /* Shift all runs from temporary lists by the computed offsets */
                for (int i = 0; i < numRL.count; i++) {
                    numRL.runs[i].offsetX += numOffX;
                    numRL.runs[i].offsetY += numY;
                    runlist_push(p->rl, numRL.runs[i]);
                }
                for (int i = 0; i < denRL.count; i++) {
                    denRL.runs[i].offsetX += denOffX;
                    denRL.runs[i].offsetY += denY;
                    runlist_push(p->rl, denRL.runs[i]);
                }

                /* Fraction bar: tile underscores which join seamlessly.
                   Use a small font size (FRAC_BAR_SZ * fracSz) for thin bar. */
                {
                    float barFontSz = fracSz * FRAC_BAR_SZ;
                    float oneW = measure_text(p->font, "_", barFontSz);
                    if (oneW > 0.0f) {
                        int count = (int)(maxW / oneW) + 1;
                        if (count < 1) count = 1;
                        char *barStr = (char *)malloc(count + 1);
                        memset(barStr, '_', count);
                        barStr[count] = '\0';
                        char *barText = alloctrk_dup(p->at, barStr);
                        free(barStr);
                        float barW = oneW * count;
                        float barX = curX + (maxW - barW) * 0.5f;
                        SlugTextRun barRun = {
                            .font     = p->font,
                            .text     = barText,
                            .fontSize = barFontSz,
                            .offsetX  = barX,
                            .offsetY  = barY,
                        };
                        runlist_push(p->rl, barRun);
                    }
                }

                free(numRL.runs);
                free(denRL.runs);
                free(numStr);
                free(denStr);
                curX += maxW;
                continue;
            }

            /* Unknown command: just skip the backslash, emit nothing */
            continue;
        }

        /* Plain text: append current UTF-8 character to buffer */
        {
            int n = utf8_char_len(p->src + p->pos, p->len - p->pos);
            strbuf_append(&p->buf, p->src + p->pos, n);
            p->pos += n;
        }
    }

    /* Flush any remaining plain text */
    curX += flush_plain(p, fontSize, curX, baseY);

    return curX - baseX;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

SlugTextData slug_prepare_math_text(const stbtt_fontinfo *fonts, int fontCount,
                                    const char *markup, float fontSize)
{
    RunList rl;
    runlist_init(&rl);

    AllocTracker at;
    alloctrk_init(&at);

    Parser parser = {
        .src       = markup,
        .len       = (int)strlen(markup),
        .pos       = 0,
        .font      = &fonts[0],
        .fonts     = fonts,
        .fontCount = fontCount,
        .rl        = &rl,
        .at        = &at,
    };
    strbuf_init(&parser.buf);

    parse_expr(&parser, fontSize, 0.0f, 0.0f);

    strbuf_free(&parser.buf);

    /* Build the SlugTextData from all collected runs */
    SlugTextData result = {0};
    if (rl.count > 0) {
        result = slug_prepare_runs(rl.runs, rl.count);
    }

    /* Clean up */
    alloctrk_free_all(&at);
    free(rl.runs);

    return result;
}

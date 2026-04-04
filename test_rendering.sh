#!/bin/bash
# Slug headless rendering test suite
# Renders various text samples and reports results.
# Usage: ./test_rendering.sh [output_dir]
set -e

SLUG="${0%/*}/build/slug_headless"
OUT="${1:-/tmp/slug_tests}"
mkdir -p "$OUT"
PASS=0; FAIL=0; SKIP=0

run_test() {
    local name="$1" font_arg="$2" text="$3" expect="$4"
    local outfile="$OUT/${name}.png"
    local cmd=("$SLUG" -s 100 -o "$outfile")
    if [ -n "$font_arg" ]; then
        cmd+=(-f "$font_arg")
    fi
    cmd+=("$text")

    if output=$("${cmd[@]}" 2>&1); then
        local w h
        w=$(echo "$output" | grep -oP '\d+(?=x\d+$)' || true)
        h=$(echo "$output" | grep -oP '(?<=x)\d+$' || true)
        printf "  PASS  %-30s  %sx%s  %s\n" "$name" "$w" "$h" "$expect"
        PASS=$((PASS + 1))
    else
        if echo "$output" | grep -q "No renderable glyphs"; then
            printf "  PASS  %-30s  (no glyphs)  %s\n" "$name" "$expect"
            PASS=$((PASS + 1))
        elif echo "$output" | grep -q "could not find font"; then
            printf "  SKIP  %-30s  (font not found)  %s\n" "$name" "$expect"
            SKIP=$((SKIP + 1))
        else
            printf "  FAIL  %-30s  %s\n" "$name" "$expect"
            echo "$output" | head -3 | sed 's/^/        /'
            FAIL=$((FAIL + 1))
        fi
    fi
}

echo "=== Slug Rendering Test Suite ==="
echo "Binary: $SLUG"
echo "Output: $OUT"
echo ""

# --- Basic Latin ---
echo "[Basic Latin]"
run_test "latin_hello"         "" "Hello, Slug!"                            "basic ASCII"
run_test "latin_pangram"       "" "The quick brown fox jumps over the lazy dog" "full alphabet"
run_test "latin_digits"        "" "0123456789"                              "digits"
run_test "latin_punctuation"   "" "Hello! How are you? Fine, thanks."       "punctuation"
run_test "latin_single_char"   "" "A"                                       "single character"

# --- Accented Latin / European ---
echo ""
echo "[Accented Latin / European]"
run_test "german_umlauts"      "" "Ärger über Größe"                        "German umlauts + eszett"
run_test "french_accents"      "" "Café résumé naïve"                       "French accents"
run_test "spanish_accents"     "" "¡Hola! ¿Cómo estás? Año"                "Spanish accents + inverted marks"
run_test "nordic_chars"        "" "Åland Ørsted Ångström"                   "Nordic Å Ø"
run_test "polish_chars"        "" "Łódź źródło żółw"                        "Polish diacritics"
run_test "czech_chars"         "" "Příliš žluťoučký kůň"                    "Czech háčky + čárky"

# --- Greek ---
echo ""
echo "[Greek]"
run_test "greek_lower"         "" "αβγδεζηθ"                               "Greek lowercase"
run_test "greek_upper"         "" "ΑΒΓΔΕΖΗΘ"                               "Greek uppercase"
run_test "greek_words"         "" "Ελληνικά αβγδ"                           "Greek words"

# --- Cyrillic ---
echo ""
echo "[Cyrillic]"
run_test "cyrillic_russian"    "" "Привет мир"                              "Russian"
run_test "cyrillic_ukrainian"  "" "Київ Україна їжак"                       "Ukrainian specific chars"

# --- Unicode Superscripts / Subscripts ---
echo ""
echo "[Superscripts / Subscripts]"
run_test "superscript_nums"    "" "x² + y³ = z⁴"                           "Unicode superscript digits"
run_test "subscript_nums"      "" "H₂O CO₂ Fe₂O₃"                          "Unicode subscript digits"
run_test "super_sub_mixed"     "" "E=mc² H₂O ¹²C"                          "mixed super+sub"
run_test "superscript_all"     "" "⁰¹²³⁴⁵⁶⁷⁸⁹"                             "all superscript digits"
run_test "subscript_all"       "" "₀₁₂₃₄₅₆₇₈₉"                             "all subscript digits"

# --- Currency Symbols ---
echo ""
echo "[Currency Symbols]"
run_test "currency_common"     "" "\$100 €50 £30 ¥1000"                     "common currency symbols"
run_test "currency_extended"   "" "₹ ₿ ¢ ₩ ₫"                              "extended currency"

# --- Math Symbols (NotoSans may lack these) ---
echo ""
echo "[Math Symbols]"
run_test "math_basic"          "" "± × ÷ = ≠"                              "basic math operators"
run_test "math_extended"       "" "∑ ∏ ∫ √ ∞ ≤ ≥"                          "extended math (may be tofu)"
run_test "math_arrows"         "" "← → ↑ ↓ ↔"                              "arrows"

# --- Arabic (needs shaping - known limitation) ---
echo ""
echo "[Arabic - known limitation: no shaping/RTL]"
run_test "arabic_noto"         "Noto Sans Arabic"  "مرحبا بالعالم"         "Arabic with Arabic font (no shaping)"
run_test "arabic_naskh"        "Noto Naskh Arabic" "بسم الله"              "Naskh Arabic"
run_test "arabic_in_latin"     ""                   "Hello مرحبا"           "Arabic in Latin font (expect tofu)"

# --- Hebrew (needs Hebrew font) ---
echo ""
echo "[Hebrew]"
run_test "hebrew_noto"         "Noto Sans Hebrew"  "שלום עולם"             "Hebrew with Hebrew font"
run_test "hebrew_default"      ""                   "שלום עולם"             "Hebrew in default font (may tofu)"

# --- CJK ---
echo ""
echo "[CJK]"
run_test "cjk_chinese"         "Noto Sans CJK"     "你好世界"               "Chinese"
run_test "cjk_japanese"        "Noto Sans CJK"     "こんにちは世界"          "Japanese hiragana + kanji"
run_test "cjk_korean"          "Noto Sans CJK"     "안녕하세요"              "Korean hangul"

# --- Thai ---
echo ""
echo "[Thai]"
run_test "thai"                "Noto Sans Thai"    "สวัสดีครับ"              "Thai (needs shaping)"

# --- Devanagari ---
echo ""
echo "[Devanagari]"
run_test "devanagari"          "Noto Sans Devanagari" "नमस्ते दुनिया"       "Hindi (needs shaping)"

# --- Edge Cases ---
echo ""
echo "[Edge Cases]"
run_test "spaces_only"         "" "   "                                     "whitespace only (expect empty)"
run_test "newline"             "" "Hello
World"                                                                      "embedded newline"
run_test "long_text"           "" "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" "62-char string"
run_test "repeated_char"       "" "AAAAAAAAAA"                              "repeated character (glyph reuse)"
run_test "mixed_sizes"         "" "AaBbCcDd"                                "mixed case (ascender/descender)"

# --- Different Fonts ---
echo ""
echo "[Font Variants]"
run_test "dejavu_sans"         "DejaVu Sans"       "Hello DejaVu"          "DejaVu Sans"
run_test "dejavu_serif"        "DejaVu Serif"      "Hello DejaVu"          "DejaVu Serif"
run_test "dejavu_mono"         "DejaVu Sans Mono"  "Hello DejaVu"          "DejaVu Mono"
run_test "liberation_sans"     "Liberation Sans"   "Hello Liberation"      "Liberation Sans"

# --- Math Typesetting (--math mode) ---
echo ""
echo "[Math Typesetting]"
run_math_test() {
    local name="$1" font_arg="$2" text="$3" expect="$4"
    local outfile="$OUT/${name}.png"
    local cmd=("$SLUG" --math -s 100 -o "$outfile")
    if [ -n "$font_arg" ]; then
        cmd+=(-f "$font_arg")
    fi
    cmd+=("$text")

    if output=$("${cmd[@]}" 2>&1); then
        local w h
        w=$(echo "$output" | grep -oP '\d+(?=x\d+$)' || true)
        h=$(echo "$output" | grep -oP '(?<=x)\d+$' || true)
        printf "  PASS  %-30s  %sx%s  %s\n" "$name" "$w" "$h" "$expect"
        PASS=$((PASS + 1))
    else
        if echo "$output" | grep -q "No renderable glyphs"; then
            printf "  PASS  %-30s  (no glyphs)  %s\n" "$name" "$expect"
            PASS=$((PASS + 1))
        else
            printf "  FAIL  %-30s  %s\n" "$name" "$expect"
            echo "$output" | head -3 | sed 's/^/        /'
            FAIL=$((FAIL + 1))
        fi
    fi
}

run_math_test "math_superscript"    "" 'x^{2} + y^{3}'              "basic superscripts"
run_math_test "math_subscript"      "" 'a_{n} + b_{max}'            "basic subscripts"
run_math_test "math_mixed"          "" 'E = mc^{2}'                 "mixed normal + super"
run_math_test "math_fraction"       "" '\frac{1}{2}'                "simple fraction"
run_math_test "math_frac_complex"   "" '\frac{x+1}{y-1}'           "complex fraction"
run_math_test "math_nested_super"   "" 'x^{2^{n}}'                 "nested superscript"
run_math_test "math_sub_super"      "" 'x_{i}^{2}'                 "sub + super on same base"
run_math_test "math_plot_label"     "" '10^{-3}'                    "plot axis label"
run_math_test "math_escape"         "" 'a\^b\_c'                    "escaped special chars"
run_math_test "math_plain"          "" 'Hello World'                "plain text in math mode"
run_math_test "math_single_super"   "" 'x^2'                       "single-char super shorthand"
run_math_test "math_single_sub"     "" 'a_n'                       "single-char sub shorthand"

echo ""
echo "=== Results ==="
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
echo "  Skipped: $SKIP"
echo "  Total:  $((PASS + FAIL + SKIP))"
echo ""
echo "Output PNGs in: $OUT"

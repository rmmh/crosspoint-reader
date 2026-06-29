#!/bin/bash

set -e

cd "$(dirname "$0")"

READER_FONT_STYLES=("Regular" "Italic" "Bold" "BoldItalic")
NOTOSERIF_FONT_SIZES=(12 14 16 18)
NOTOSANS_FONT_SIZES=(12 14 16 18)

for size in ${NOTOSERIF_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="notoserif_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/NotoSerif/NotoSerif-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress --pnum > $output_path
    echo "Generated $output_path"
  done
done

for size in ${NOTOSANS_FONT_SIZES[@]}; do
  for style in ${READER_FONT_STYLES[@]}; do
    font_name="notosans_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/NotoSans/NotoSans-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path --2bit --compress --pnum > $output_path
    echo "Generated $output_path"
  done
done

UI_FONT_SIZES=(10 12)
UI_FONT_STYLES=("Regular" "Bold")

for size in ${UI_FONT_SIZES[@]}; do
  for style in ${UI_FONT_STYLES[@]}; do
    font_name="ubuntu_${size}_$(echo $style | tr '[:upper:]' '[:lower:]')"
    font_path="../builtinFonts/source/Ubuntu/Ubuntu-${style}.ttf"
    hebrew_path="../builtinFonts/source/NotoSansHebrew/NotoSansHebrew-${style}.ttf"
    # Ubuntu lacks the Latin Extended Additional block (U+1EA0-U+1EF9) used for
    # Vietnamese tone marks. Append a Vietnamese-only Ubuntu cut so those glyphs
    # are filled from it while every glyph Ubuntu already has stays unchanged
    # (fontstack is ordered by descending priority).
    viet_path="../builtinFonts/source/Ubuntu/Ubuntu-Vietnamese-${style}.ttf"
    output_path="../builtinFonts/${font_name}.h"
    python fontconvert.py $font_name $size $font_path $hebrew_path $viet_path \
      --additional-intervals 0x05D0,0x05EA > $output_path
    echo "Generated $output_path"
  done
done

python fontconvert.py notosans_8_regular 8 \
  ../builtinFonts/source/NotoSans/NotoSans-Regular.ttf \
  ../builtinFonts/source/NotoSansHebrew/NotoSansHebrew-Regular.ttf \
  --additional-intervals 0x05D0,0x05EA > ../builtinFonts/notosans_8_regular.h

# IPA font — Doulos SIL Regular, 16pt, IPA codepoints only
IPA_SOURCE="../builtinFonts/source/DoulosSIL/DoulosSIL-Regular.ttf"
NOTOSERIF_SOURCE="../builtinFonts/source/NotoSerif/NotoSerif-Regular.ttf"

# Symbols font — Noto Sans Symbols 2 Regular (downloaded from releases if missing)
SYMBOLS_DIR="../builtinFonts/source/NotoSansSymbols2"
SYMBOLS_SOURCE="${SYMBOLS_DIR}/NotoSansSymbols2-Regular.ttf"

if [ ! -f "$SYMBOLS_SOURCE" ]; then
  echo "Downloading NotoSansSymbols2-Regular.ttf from releases..."
  mkdir -p "$SYMBOLS_DIR"
  TEMP_ZIP="/tmp/NotoSansSymbols2.zip"
  TEMP_DIR="/tmp/NotoSansSymbols2_extract"
  
  curl -L -o "$TEMP_ZIP" "https://github.com/notofonts/symbols/releases/download/NotoSansSymbols2-v2.008/NotoSansSymbols2-v2.008.zip"
  mkdir -p "$TEMP_DIR"
  unzip -q -o "$TEMP_ZIP" -d "$TEMP_DIR"
  
  FOUND_TTF=$(find "$TEMP_DIR" -name "NotoSansSymbols2-Regular.ttf" | head -n 1)
  if [ -n "$FOUND_TTF" ]; then
    cp "$FOUND_TTF" "$SYMBOLS_SOURCE"
    echo "Installed NotoSansSymbols2-Regular.ttf successfully."
  else
    echo "Error: NotoSansSymbols2-Regular.ttf not found in the release archive"
    exit 1
  fi
  rm -rf "$TEMP_ZIP" "$TEMP_DIR"
fi

python fontconvert.py ipa_16_regular 16 "$IPA_SOURCE" "$SYMBOLS_SOURCE" "$NOTOSERIF_SOURCE" \
  --2bit --compress \
  --no-default-intervals \
  --additional-intervals 0x0220,0x02FF \
  --additional-intervals 0x0300,0x036F \
  --additional-intervals 0x0370,0x03FF \
  --additional-intervals 0x1D00,0x1DBF \
  --additional-intervals 0x1DC0,0x1DFF \
  --additional-intervals 0x1E00,0x1FFF \
  --additional-intervals 0x20D0,0x20FF \
  --additional-intervals 0x2153,0x2154 \
  --additional-intervals 0x2192,0x2192 \
  --additional-intervals 0x221A,0x221A \
  --additional-intervals 0x266D,0x266F \
  --additional-intervals 0x261E,0x261E \
  > ../builtinFonts/ipa_16_regular.h

echo "Generated ipa_16_regular.h"

echo ""
echo "Running compression verification..."
python verify_compression.py ../builtinFonts/

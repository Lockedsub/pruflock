#!/bin/bash
# SPDX-License-Identifier: MIT
# PrufLock (https://github.com/Lockedsub/pruflock)
# Author: LockedSub, Zürich, Switzerland
# Exports the printable STL parts, the 3MF plates and preview images from the OpenSCAD model.

# Usage: ./build.sh  → stl/shell.stl, stl/lid.stl, stl/clip.stl,
#                      "PrufLock Case.3mf", "PrufLock Case with Clip.3mf",
#                      preview/{front,back,back_noclip,illustration,explode,wiring}.png
set -euo pipefail
cd "$(dirname "$0")"
OPENSCAD=${OPENSCAD:-/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD}
mkdir -p stl preview

for p in shell lid clip; do
  echo "== $p"
  "$OPENSCAD" --backend=manifold -o "stl/$p.stl" -D "part=\"print_$p\"" pruflock_case.scad 2>&1 \
    | grep -E "WARNING|ERROR|Volumes|Simple" || true
done

# 3MFs with all parts on one plate, ready for the slicer: lazy-union keeps each part an object of its
# own; 3mf_parts.py names them and makes lid + logo one object with two parts (two filaments).
plate() {  # file, with_clip, title, names...
  local file=$1 clip=$2 title=$3; shift 3
  echo "== $file"
  "$OPENSCAD" --backend=manifold --enable=lazy-union -o "$file" -D with_clip="$clip" -D 'part="plate"' \
    -O export-3mf/color-mode=model -O "export-3mf/meta-data-title=$title" \
    -O export-3mf/meta-data-license-terms=MIT pruflock_case.scad 2>&1 | grep -E "WARNING|ERROR" || true
  python3 -I 3mf_parts.py "$file" "$@"
}
plate "PrufLock Case.3mf"           false "PrufLock case"                Shell Lid+Logo
plate "PrufLock Case with Clip.3mf" true  "PrufLock case with belt clip" Shell Lid Clip

# OpenSCAD's PNG export isn't anti-aliased, so render at SS × the size and scale down
# (ImageMagick if installed, else macOS sips). SS=1 skips that. With ImageMagick, the
# background (the Tomorrow scheme's flat light grey) also becomes white, for printing.
SS=${SS:-3}
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
render() {  # name, part, camera, [image size], [extra options]; env FILE = .scad file, TRIM=1 crops margins (ImageMagick)
  local size=${4:-1600,1000} w h
  w=${size%,*}; h=${size#*,}
  "$OPENSCAD" -o "$TMP/$1.png" --imgsize="$((w * SS)),$((h * SS))" --colorscheme=Tomorrow \
    --camera="$3" -D "part=\"$2\"" ${5:-} "${FILE:-pruflock_case.scad}" >/dev/null 2>&1
  if command -v magick >/dev/null; then
    magick "$TMP/$1.png" -fill white -opaque "srgb(248,248,248)" -filter Lanczos -resize "${w}x${h}" \
      $([ "${TRIM:-0}" = 1 ] && echo "-trim +repage -bordercolor white -border 30") "preview/$1.png"
  elif [ "$SS" = 1 ]; then mv "$TMP/$1.png" "preview/$1.png"
  else sips -z "$h" "$w" "$TMP/$1.png" --out "preview/$1.png" >/dev/null
  fi
}
render front    assembly "0,0,0,55,0,25,150"
render back     assembly "0,0,0,125,0,205,150"
render back_noclip assembly "0,0,0,125,0,205,150" 1600,1000 "-D with_clip=false"
render illustration illustration "1,1,-3,38,0,18,150"
render explode  explode  "0,0,0,62,0,150,300" 1000,1400 "--viewall --autocenter"   # needs NopSCADlib
FILE=pruflock_wiring.scad TRIM=1 \
render wiring   wiring   "0,0,0,0,0,0,100" 1300,1200 "--viewall --autocenter --projection=o"
echo "done: stl/ *.3mf preview/"

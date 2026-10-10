#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com>
# The guide's screenshots: every screen rendered on the host, 2x nearest, to docs/img/screen-*.png.
#   sh tools/docs_img.sh        (after sh build.sh: needs build/gen)
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d)
cc -O2 -w -Ibuild/gen -o "$T/screen" tests/screen.c
"$T/screen" "$T" docs docs/CONTROLS.md >/dev/null
mkdir -p docs/img
rm -f docs/img/screen-*.png
python3 - "$T" docs/img <<'PY'
import glob, os, sys
from PIL import Image
for p in sorted(glob.glob(os.path.join(sys.argv[1], "*.ppm"))):
    im = Image.open(p)
    im.resize((480, 480), Image.NEAREST).save(os.path.join(sys.argv[2], "screen-" + os.path.basename(p)[:-4] + ".png"))
PY
rm -rf "$T"
ls docs/img/screen-*.png | wc -l

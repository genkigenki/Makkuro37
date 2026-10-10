#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com>
# The guide's panel images: tools/panel.html in headless Chrome -> docs/img/panel-1..8.png, panel-map.png.
#   CHROME=/path/to/chrome sh tools/docs_panels.sh
set -e
cd "$(dirname "$0")/.."
CHROME="${CHROME:-chrome}"
URL="file://$(pwd)/tools/panel.html"
mkdir -p docs/img
for l in 1 2 3 4 5 6 7 8 map; do
    "$CHROME" --headless=new --disable-gpu --hide-scrollbars --window-size=1600,680 \
        --screenshot="docs/img/panel-$l.png" "$URL?l=$l" >/dev/null 2>&1
done
ls docs/img/panel-*.png
# the README's hero: the boot screen beside the controls map
python3 - <<'PY'
from PIL import Image
boot, panel = Image.open("docs/img/screen-boot.png"), Image.open("docs/img/panel-map.png").convert("RGB")
w = round(panel.width * 480 / panel.height)
hero = Image.new("RGB", (480 + 24 + w, 480))
hero.paste(boot, (0, 0))
hero.paste(panel.resize((w, 480), Image.LANCZOS), (504, 0))
hero.save("docs/img/hero.png")
PY

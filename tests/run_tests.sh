#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com>
# Host tests (no hardware), from the repo root:  tests/run_tests.sh [OUTDIR]
#   host    the engine: renders to OUTDIR/*.wav, level / clip / DC checks, cost per block
#   screen  the screen to OUTDIR/*.png
#   board   the flash store, update loader, OTA entry, recovery (board layer tests)
set -e
cd "$(dirname "$0")/.."
OUT=build/host
DIR="${1:-$OUT}"
mkdir -p "$OUT" build/gen "$DIR"
python3 tools/gen_tables.py build/gen/mk_tables.h >/dev/null
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
CC="${CC:-cc} -O2 -Wall -Wno-unused-function"
fail=0
$CC -Ibuild/gen -o "$OUT/host" tests/host.c -lm
"$OUT/host" "$DIR" || fail=1
$CC -w -Ibuild/gen -o "$OUT/screen" tests/screen.c
"$OUT/screen" "$DIR" >/dev/null || fail=1
python3 - "$DIR" <<'EOF' || fail=1
import glob, os, sys
from PIL import Image
for p in glob.glob(os.path.join(sys.argv[1], "*.ppm")):
    Image.open(p).save(p[:-4] + ".png")
    os.remove(p)
print("  screens: " + ", ".join(sorted(os.path.basename(p) for p in glob.glob(os.path.join(sys.argv[1], "*.png")))))
EOF
PKG=build/makkuro37.fwsc
[ -f $PKG ] || { echo "run ./build.sh first"; exit 1; }
for t in "storage_test" "smk_flash_test" "owner_test" "recovery_test" "ota_test $PKG" "ldr_test ${OLD_PKG:-$PKG} $PKG"; do
    set -- $t
    $CC -w -DBOARD_SMK37=1 -Ifirmware/hal -Ifirmware/src -Ibuild/gen -o "$OUT/$1" "tests/$1.c" &&
        "$OUT/$1" $2 $3 >"$OUT/$1.log" 2>&1 && echo "  ok   $1" || { echo "  FAIL $1 (build/host/$1.log)"; fail=1; }
done
[ $fail = 0 ] && echo "all tests passed" || { echo "TESTS FAILED"; exit 1; }

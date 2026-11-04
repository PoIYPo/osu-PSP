#!/usr/bin/env bash
# Packages a PSP ELF into EBOOT.PBP using pspsdk's tools.
#
#   scripts/package_psp.sh <elf> <output.pbp>
#
# pspsdk ships two SFO generators whose CLI drifted across releases (mksfo,
# mksfoex), so we try both and validate the magic instead of trusting the exit
# code. pack-pbp takes 8 slots: PARAM.SFO, ICON0, ICON1, PIC0, PIC1, SND0,
# DATA.PSP, DATA.PSAR -- unused slots are the literal string NULL.
set -euo pipefail

ELF="${1:-build-psp/osu_psp.elf}"
OUT="${2:-build-psp/EBOOT.PBP}"
TITLE="osu!psp"
TITLE_ID="OSUPSP001"

if [ ! -f "$ELF" ]; then
  echo "error: ELF not found: $ELF" >&2
  exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
SFO="$WORK/PARAM.SFO"

sfo_ok() {
  [ -s "$1" ] && [ "$(head -c 4 "$1" | od -An -tx1 | tr -d ' \n')" = "00505346" ]
}

if command -v mksfoex >/dev/null 2>&1; then
  echo "--- mksfoex -d $TITLE_ID $TITLE ---"
  mksfoex -d "$TITLE_ID" "$TITLE" "$SFO" || true
fi

if ! sfo_ok "$SFO" && command -v mksfo >/dev/null 2>&1; then
  echo "--- mksfo $TITLE ---"
  mksfo "$TITLE" "$SFO" || true
fi

if ! sfo_ok "$SFO"; then
  echo "error: could not generate PARAM.SFO" >&2
  (mksfoex -h || true) 2>&1 | head -20
  (mksfo -h || true) 2>&1 | head -20
  exit 1
fi

pack-pbp "$OUT" "$SFO" NULL NULL NULL NULL NULL "$ELF" NULL
ls -l "$OUT"
echo "built $OUT"

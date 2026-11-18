#!/bin/bash
# Assemble nhl94s.zip from this tree. Optional: copy an aarch64 binary in.
#   ./portmaster/pack.sh [path/to/nhl94s.aarch64]
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/out/nhl94s.zip}"
STAGE="$ROOT/out/portmaster-nhl94s"
BIN="${1:-}"

rm -rf "$STAGE"
mkdir -p "$STAGE/nhl94s/roms" "$STAGE/nhl94s/conf" "$STAGE/nhl94s/libs.aarch64" "$STAGE/nhl94s/licenses"
cp "$ROOT/portmaster/NHL 94 SNES.sh" "$STAGE/"
chmod +x "$STAGE/NHL 94 SNES.sh"
cp "$ROOT/portmaster/nhl94s/port.json" "$ROOT/portmaster/nhl94s/gameinfo.xml" \
   "$ROOT/portmaster/nhl94s/nhl94s.gptk" "$ROOT/portmaster/nhl94s/README.md" "$STAGE/nhl94s/"
cp "$ROOT/portmaster/nhl94s/licenses/NOTICE.txt" "$STAGE/nhl94s/licenses/"
cp "$ROOT/portmaster/NHL 94 SNES.sh" "$STAGE/nhl94s/"
if [ -n "$BIN" ]; then
  cp "$BIN" "$STAGE/nhl94s/nhl94s.aarch64"
  chmod +x "$STAGE/nhl94s/nhl94s.aarch64"
fi
mkdir -p "$(dirname "$OUT")"
(cd "$STAGE" && zip -rq "$OUT" "NHL 94 SNES.sh" nhl94s)
echo "wrote $OUT"

#!/usr/bin/env sh
# =====================================================================
#  Heart of The Alien Redux 2.0.3 - Unix launcher
#
#  Opens the cross-platform setup screen (play.py). Extra arguments
#  start the game immediately:
#
#      ./play.sh                 setup screen
#      ./play.sh --setup         setup screen (explicit)
#      ./play.sh --fullscreen    start with extra options
# =====================================================================
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT" || exit 1

if command -v python3 >/dev/null 2>&1; then
    exec python3 "$ROOT/play.py" "$@"
fi

# Fallback for systems without Python: launch with sensible defaults.
GAME="$ROOT/alien"
if [ ! -x "$GAME" ] && [ -x "$ROOT/build/alien" ]; then
    GAME="$ROOT/build/alien"
fi

if [ ! -x "$GAME" ]; then
    echo "ERROR: alien was not found. Build it first (cmake -B build && cmake --build build)." >&2
    exit 1
fi

exec "$GAME" --iso --hd --hd-smooth 30 --music-volume 100 --sfx-volume 100 --skip-password "$@"

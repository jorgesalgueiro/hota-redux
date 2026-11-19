#!/usr/bin/env bash
# Run the Linux ASan/UBSan build headlessly against the game data.
set -u

BIN="${1:-$HOME/hota-asan/alien}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DATA_DIR="${DATA_DIR:-$REPO/Heart Of The Alien (U)}"
WORK="${WORK:-$HOME/hota-work}"
LOG_DIR="${LOG_DIR:-$HOME/asan-logs}"
REPLAY_SECS="${REPLAY_SECS:-10}"

export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

rm -rf "$WORK"
mkdir -p "$WORK" "$LOG_DIR"
ln -s "$DATA_DIR"/*.iso "$WORK/" 2>/dev/null
ln -s "$DATA_DIR"/*.mp3 "$WORK/" 2>/dev/null
cd "$WORK" || exit 1

check() {
    local name="$1"
    local log="$LOG_DIR/$name.out"
    shift
    timeout "$@" >"$log" 2>&1
    local code=$?
    echo "=== $name exit=$code"
    if grep -q -e 'runtime error' -e 'AddressSanitizer' -e 'panic' "$log"; then
        grep -e 'runtime error' -e 'AddressSanitizer' -e 'panic' "$log" | head -5
    fi
}

# room runs (timeout means still running, which is what we expect)
for r in 1 2 3 4 5 6 7 8; do
    check "room$r" 3 "$BIN" --iso --room "$r" --fastest
done

# every internal upscaling path
for u in 1 2 3 4; do
    check "upscale$u" 3 "$BIN" --iso --room 1 --upscale "$u" --fastest
done

check "smooth-upscale" 3 "$BIN" --iso --room 1 --upscale 4 --smooth --fastest

# key bindings file parsing (exits immediately on --check-keys)
check "keys-default" 3 "$BIN" --check-keys
check "keys-sample" 3 "$BIN" --check-keys --keys "$REPO/tests/keys-sample.txt"
check "keys-room" 3 "$BIN" --iso --room 1 --keys "$REPO/tests/keys-sample.txt" --fastest

# recorded key sequences
for spec in level1:1 level2:2 boss:7; do
    name="${spec%%:*}"
    room="${spec##*:}"
    cp "$REPO/tests/recorded-keys.$name" recorded-keys
    check "replay-$name" "$REPLAY_SECS" "$BIN" --iso --room "$room" --replay --fastest
done

# animations run to completion (exit code 0)
timeout 120 "$BIN" --iso --upscale 4 --intro-test --fastest >"$LOG_DIR/intro.out" 2>&1
echo "=== intro exit=$?"
grep -e 'runtime error' -e 'AddressSanitizer' -e 'panic' "$LOG_DIR/intro.out" | head -5

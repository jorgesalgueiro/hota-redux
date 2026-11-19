#!/usr/bin/env bash
# Smoke/regression tests for the Linux build.
# Game data is expected in ../Heart Of The Alien (U).
#
#   BIN=build/alien DATA_DIR="Heart Of The Alien (U)" ./tests/run-tests-linux.sh
#
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
BIN=${BIN:-$REPO/build/alien}
DATA_DIR=${DATA_DIR:-$REPO/Heart Of The Alien (U)}
WORK=${WORK:-$HOME/hota-linux-work}
ISO_PREFIX=${ISO_PREFIX:-$DATA_DIR/Heart Of The Alien (U)}
IDLE_SECONDS=${IDLE_SECONDS:-2}
REPLAY_SECONDS=${REPLAY_SECONDS:-8}

export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy}
export SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy}

rm -rf "$WORK"
mkdir -p "$WORK"

passes=0
failures=0

check() {
    local name=$1
    local seconds=$2
    local expected=$3
    shift 3

    local log="$WORK/$name.log"
    (cd "$WORK" && timeout "$seconds" "$BIN" "$@") >"$log" 2>&1
    local code=$?

    if grep -Eq -e 'panic' -e 'AddressSanitizer' -e 'runtime error' \
        -e 'unknown opcode' -e 'UNKNOWN' -e 'Segmentation' "$log"; then
        echo "FAIL $name (reported an error)"
        grep -E -e 'panic' -e 'AddressSanitizer' -e 'runtime error' \
            -e 'unknown opcode' -e 'UNKNOWN' -e 'Segmentation' "$log" | head -3
        failures=$((failures + 1))
        return
    fi

    if [ "$expected" = "run" ]; then
        if [ "$code" -ne 124 ] && [ "$code" -ne 0 ]; then
            echo "FAIL $name (exit $code)"
            failures=$((failures + 1))
            return
        fi
    elif [ "$code" -ne "$expected" ]; then
        echo "FAIL $name (exit $code, expected $expected)"
        failures=$((failures + 1))
        return
    fi

    echo "PASS $name"
    passes=$((passes + 1))
}

if [ ! -x "$BIN" ]; then
    echo "executable not found: $BIN" >&2
    exit 1
fi

if [ ! -f "$ISO_PREFIX.iso" ]; then
    echo "ISO not found: $ISO_PREFIX.iso" >&2
    exit 1
fi

check help 10 0 --help
check room-validation 10 1 --room 9
check upscale-validation 10 1 --upscale 5
check hd-smooth-validation 10 1 --hd-smooth 101

check intro 180 0 --iso "$ISO_PREFIX" --intro-test --fastest --no-sound

for room in 1 2 3 4 5 6 7 8; do
    check "sprite-room$room" $IDLE_SECONDS run \
        --iso "$ISO_PREFIX" --room "$room" --sprite-test --fastest --no-sound
    check "idle-room$room" $IDLE_SECONDS run \
        --iso "$ISO_PREFIX" --room "$room" --fastest --no-sound
done

for factor in 1 2 3 4; do
    check "upscale-$factor" $IDLE_SECONDS run \
        --iso "$ISO_PREFIX" --room 1 --upscale "$factor" --fastest --no-sound
done

check integer-scaling $IDLE_SECONDS run \
    --iso "$ISO_PREFIX" --room 1 --integer-scale --fastest --no-sound
check volume-options $IDLE_SECONDS run \
    --iso "$ISO_PREFIX" --room 1 --music-volume 50 --sfx-volume 50 --fastest --no-sound
check debug-logging $IDLE_SECONDS run \
    --iso "$ISO_PREFIX" --room 1 --debug --fastest --no-sound
if ! grep -q 'loading ROOMS1' "$WORK/debug-logging.log"; then
    echo "FAIL debug-logging (no log output)"
    failures=$((failures + 1))
    passes=$((passes - 1))
fi

check keys-default 10 0 --check-keys
check keys-sample 10 0 --check-keys --keys "$REPO/tests/keys-sample.txt"

for spec in level1:1 level2:2 boss:7 gameover-start:1; do
    name=${spec%%:*}
    room=${spec##*:}
    cp "$REPO/tests/recorded-keys.$name" "$WORK/recorded-keys"
    check "replay-$name" "$REPLAY_SECONDS" run \
        --iso "$ISO_PREFIX" --room "$room" --replay --fastest --no-sound
done

mkdir -p "$WORK/Coração"
if [ ! -f "$WORK/Coração/Coração.iso" ]; then
    ln "$ISO_PREFIX.iso" "$WORK/Coração/Coração.iso" 2>/dev/null ||
        cp "$ISO_PREFIX.iso" "$WORK/Coração/Coração.iso"
fi
check utf8-path $IDLE_SECONDS run \
    --iso "Coração/Coração" --room 1 --fastest --no-sound

if command -v python3 >/dev/null 2>&1; then
    out=$(python3 "$REPO/play.py" --print-options 2>&1)
    if printf '%s\n' "$out" | grep -q '^ARGS:'; then
        echo "PASS launcher-python"
        passes=$((passes + 1))
    else
        echo "FAIL launcher-python"
        failures=$((failures + 1))
    fi

    out=$(sh "$REPO/play.sh" --print-options 2>&1)
    if printf '%s\n' "$out" | grep -q '^ARGS:'; then
        echo "PASS launcher-shell"
        passes=$((passes + 1))
    else
        echo "FAIL launcher-shell"
        failures=$((failures + 1))
    fi
fi

echo ""
echo "$passes passed, $failures failed"
[ "$failures" -eq 0 ]

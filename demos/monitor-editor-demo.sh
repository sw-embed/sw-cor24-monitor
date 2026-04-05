#!/bin/bash
# monitor-editor-demo.sh -- Full-stack demo: monitor -> sws -> swye -> sws
#
# Loads four COR24 binaries in the emulator:
#   monitor  at 0x000000  -- resident monitor (boot, services, registry)
#   sws      at 0x020000  -- script interpreter / shell
#   swye     at 0x040000  -- yocto-ed text editor
#   textfile at 0x010000  -- ASCII buffer for editor to load
#   cmds     at 0x0F0000  -- pre-loaded editor keystrokes
#
# The monitor boots, detects sws, and passes control.
# Staged UART input feeds sws commands to launch the editor.
# The editor reads pre-loaded keystrokes (not UART), edits the
# text buffer, quits, and returns to sws with output at 0x0F0400.
# sws displays the edited result.

set -e

# Pass --dump to get full memory hex dump after execution
EXTRA_FLAGS=""
for arg in "$@"; do
    case "$arg" in
        --dump) EXTRA_FLAGS="$EXTRA_FLAGS --dump" ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SWS_DIR="$PROJECT_DIR/../sw-cor24-script"
SWYE_DIR="$PROJECT_DIR/../sw-cor24-yocto-ed"
TC24R_INCLUDE="$HOME/github/sw-embed/sw-cor24-x-tinyc/include"
BUILD="$PROJECT_DIR/build"

mkdir -p "$BUILD"

# --- Check prerequisites ---
for tool in tc24r cor24-run; do
    if ! command -v "$tool" &>/dev/null; then
        echo "error: $tool not found in PATH"
        exit 1
    fi
done

if [ ! -f "$SWS_DIR/src/sws.c" ]; then
    echo "error: sw-cor24-script not found at $SWS_DIR"
    exit 1
fi

if [ ! -d "$SWYE_DIR/src" ]; then
    echo "error: sw-cor24-yocto-ed not found at $SWYE_DIR"
    exit 1
fi

# --- Build monitor ---
echo "Building monitor..."
tc24r "$PROJECT_DIR/src/monitor.c" -o "$BUILD/monitor_c.s" \
    -I "$TC24R_INCLUDE" -I "$PROJECT_DIR/src"
cat "$PROJECT_DIR/src/boot.s" "$BUILD/monitor_c.s" "$PROJECT_DIR/src/trampoline.s" \
    > "$BUILD/monitor.s"
cor24-run --assemble "$BUILD/monitor.s" "$BUILD/monitor.bin" "$BUILD/monitor.lst" 2>&1 | head -1

# --- Build sws at 0x020000 ---
echo "Building sws at 0x020000..."
tc24r "$SWS_DIR/src/sws.c" -o "$BUILD/sws_demo.s" -I "$SWS_DIR/include/" 2>&1 | head -1
cor24-run --assemble "$BUILD/sws_demo.s" "$BUILD/sws_demo.bin" "$BUILD/sws_demo.lst" \
    --base-addr 0x020000 2>&1 | head -1

# --- Build swye at 0x040000 ---
echo "Building swye at 0x040000..."
tc24r "$SWYE_DIR/src/swye.c" -o "$BUILD/swye_demo.s" 2>&1 | head -1
cor24-run --assemble "$BUILD/swye_demo.s" "$BUILD/swye_demo.bin" "$BUILD/swye_demo.lst" \
    --base-addr 0x040000 2>&1 | head -1

# Find swye._main address from listing (label is on its own line,
# address is on the next instruction line)
SWYE_MAIN=$(grep -A1 '_main:' "$BUILD/swye_demo.lst" | tail -1 | grep -o '^[0-9a-f]*')
if [ -z "$SWYE_MAIN" ]; then
    echo "error: could not find _main in swye listing"
    exit 1
fi
echo "swye._main at 0x${SWYE_MAIN}"

# --- Prepare test file ---
TESTFILE="$SWYE_DIR/tests/testfile.txt"
if [ ! -f "$TESTFILE" ]; then
    echo "error: test file not found at $TESTFILE"
    exit 1
fi

# --- Editor commands: move to "red", delete 3 chars, insert "blue", quit ---
# ESC enters command mode, then: move right 10 chars (past "The quick "),
# delete 3 chars ("red"), ESC back to edit, type "blue", ESC + quit
printf '\x1b10 right\ndel\ndel\ndel\n\x1bblue\x1bquit\n' > "$BUILD/swye_cmds.bin"

# --- sws script (staged UART input) ---
SWS_SCRIPT="$(cat "$SCRIPT_DIR/editor-test.sws")"

echo ""
echo "=== Input text ==="
cat "$TESTFILE"
echo ""
echo "Edit plan: replace 'red' with 'blue'"
echo ""
echo "=== Running: monitor -> sws -> swye ==="
echo ""

# --- Run everything ---
OUTPUT=$(cor24-run \
    --load-binary "$BUILD/monitor.bin@0" \
    --load-binary "$BUILD/sws_demo.bin@0x020000" \
    --load-binary "$BUILD/swye_demo.bin@0x040000" \
    --load-binary "$TESTFILE@0x010000" \
    --load-binary "$BUILD/swye_cmds.bin@0x0F0000" \
    --patch "0x0FFE00=0x0${SWYE_MAIN}" \
    --entry 0 \
    --speed 0 \
    --stack-kilobytes 8 \
    -u "${SWS_SCRIPT}"$'\x04' \
    $EXTRA_FLAGS \
    -t 30 2>&1)

# --- Display output ---
if [[ "$EXTRA_FLAGS" == *"--dump"* ]]; then
    # Full output: UART log + CPU state + memory hex dump
    echo "$OUTPUT"
else
    # Filtered: just the interesting UART lines
    echo "=== UART Output ==="
    echo "$OUTPUT" | grep -A 9999 '^UART output:' | tail -n +2 | \
        grep -v '^Executed\|^CPU\|^TEXT>\|^MODE>\|^CMD >' | \
        sed 's/^sws> sws> .*/sws> .../' | uniq | \
        grep -v '^[[:space:]]*$' | head -30
    echo ""
fi

# --- Verify ---
if echo "$OUTPUT" | grep -q 'The quick blue fox'; then
    echo ""
    echo "PASS: monitor -> sws -> swye edited red->blue -> sws displayed result"
else
    echo ""
    echo "FAIL: edit not applied or output not captured"
    echo ""
    echo "Full output (for debugging):"
    echo "$OUTPUT" | tail -40
    exit 1
fi

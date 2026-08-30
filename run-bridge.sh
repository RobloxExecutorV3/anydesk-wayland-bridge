#!/usr/bin/env bash
set -euo pipefail

LIBDIR=/usr/lib/anydesk-wayland-bridge
VDISPLAY=:99
XVFB_LOG="${XDG_RUNTIME_DIR:-/tmp}/anydesk-wayland-xvfb.log"
VIDEO_LOG="${XDG_RUNTIME_DIR:-/tmp}/anydesk-wayland-video.log"
INPUT_LOG="${XDG_RUNTIME_DIR:-/tmp}/anydesk-wayland-input.log"

cleanup() {
    trap - EXIT INT TERM
    for pid in "${INPUT_PID:-}" "${VIDEO_PID:-}" "${XVFB_PID:-}"; do
        [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
    done
    wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

GEOMETRY=$(hyprctl monitors 2>/dev/null | awk '/^[[:space:]]*[0-9]+x[0-9]+@/{split($1,a,"@"); print a[1]; exit}')
GEOMETRY=${GEOMETRY:-1920x1080}

rm -f "$XVFB_LOG" "$VIDEO_LOG" "$INPUT_LOG"
/usr/bin/Xvfb "$VDISPLAY" -screen 0 "${GEOMETRY}x24" -nolisten tcp -ac -noreset >"$XVFB_LOG" 2>&1 &
XVFB_PID=$!

for _ in {1..60}; do
    if DISPLAY="$VDISPLAY" /usr/bin/xrandr --query >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done
DISPLAY="$VDISPLAY" /usr/bin/xrandr --query >/dev/null 2>&1

PRIMARY_OUTPUT=$(DISPLAY="$VDISPLAY" /usr/bin/xrandr --query | awk '/ connected/{print $1; exit}')
if [[ -n "$PRIMARY_OUTPUT" ]]; then
    DISPLAY="$VDISPLAY" /usr/bin/xrandr --output "$PRIMARY_OUTPUT" --primary
fi

export DISPLAY="$VDISPLAY"
export QT_QPA_PLATFORM=xcb
# Xvfb has no GPU acceleration, so Mesa falls back to llvmpipe. Limit it to
# one worker by default so the bridge cannot saturate every CPU core.
export LP_NUM_THREADS="${ANYDESK_BRIDGE_RENDER_THREADS:-1}"
export ANYDESK_WAYLAND_BRIDGE=1
export XDG_SESSION_TYPE=wayland
export XDG_CURRENT_DESKTOP="${XDG_CURRENT_DESKTOP:-Hyprland}"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=$XDG_RUNTIME_DIR/bus}"

"$LIBDIR/xwaylandvideobridge" >"$VIDEO_LOG" 2>&1 &
VIDEO_PID=$!
"$LIBDIR/input-relay" >"$INPUT_LOG" 2>&1 &
INPUT_PID=$!

printf 'AnyDesk Wayland bridge: display=%s geometry=%s Xvfb=%s video=%s input=%s\n' \
    "$VDISPLAY" "$GEOMETRY" "$XVFB_PID" "$VIDEO_PID" "$INPUT_PID"

wait -n "$XVFB_PID" "$VIDEO_PID" "$INPUT_PID"

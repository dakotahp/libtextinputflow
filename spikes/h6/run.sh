#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$DIR/results.jsonl}
CONFIG=$(mktemp)
trap 'rm -f "$CONFIG"' EXIT

cat > "$CONFIG" <<EOF
xwayland disable
exec bash "$DIR/session.sh" "$OUT"
EOF

unset WAYLAND_DISPLAY DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1

timeout 120 dbus-run-session -- sway --unsupported-gpu -c "$CONFIG"
cat "$OUT"

#!/usr/bin/env bash
set -uo pipefail

OUT=$1
exec 2> "$OUT/session.log"
trap 'swaymsg exit' EXIT

export GDK_BACKEND=wayland QT_QPA_PLATFORM=wayland QT_PLUGIN_PATH="$OUT"

QT_QPA_PLATFORMTHEME=gtk3 "$OUT/themedump" > "$OUT/gtk3.txt"
QT_QPA_PLATFORMTHEME=tifwrap "$OUT/themedump" > "$OUT/tifwrap.txt"

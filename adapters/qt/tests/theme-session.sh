#!/usr/bin/env bash
set -uo pipefail

BUILD=$1 OUT=$2 STUB=$3
exec 2> "$OUT/session.log"
trap 'swaymsg exit' EXIT

export GDK_BACKEND=wayland QT_QPA_PLATFORM=wayland QT_PLUGIN_PATH="$BUILD/adapters/qt"
export XDG_CONFIG_HOME="$STUB" XDG_DATA_DIRS="$STUB:/usr/local/share:/usr/share"

QT_QPA_PLATFORMTHEME=gtk3 "$BUILD/adapters/qt/tests/themedump" > "$OUT/gtk3.txt"
QT_QPA_PLATFORMTHEME=textinputflow "$BUILD/adapters/qt/tests/themedump" > "$OUT/textinputflow.txt"

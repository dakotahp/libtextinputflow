#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$DIR/build}
mkdir -p "$OUT"
"$DIR/build.sh" "$OUT" >/dev/null

CONFIG=$(mktemp)
trap 'rm -f "$CONFIG"' EXIT
printf 'xwayland disable\nexec bash "%s" "%s"\n' "$DIR/session.sh" "$OUT" > "$CONFIG"

(
  unset WAYLAND_DISPLAY DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME
  export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
  timeout 60 dbus-run-session -- sway -c "$CONFIG" >/dev/null 2>&1
)

echo "== theme values: gtk3 vs tifwrap"
diff "$OUT/gtk3.txt" "$OUT/tifwrap.txt" && echo "identical except where listed above"
echo "($(wc -l < "$OUT/gtk3.txt") values compared)"

echo "== caret keys through tifwrap"
H6_STEPS="$DIR/../h1/steps.txt" H6_QT_THEME=tifwrap QT_PLUGIN_PATH="$OUT" \
  "$DIR/../h6/run.sh" "$OUT/keys.jsonl" >/dev/null 2>&1
grep '"toolkit":"qt"' "$OUT/keys.jsonl" | sed -E \
  -e 's/\{"toolkit":"qt","widget":"([^"]*)","step":"([^"]*)","result":/\1 \2 => /' \
  -e 's/\}$//'

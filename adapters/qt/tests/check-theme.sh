#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../../.." && pwd)
BUILD=$(cd "${1:?usage: check-theme.sh <meson build dir>}" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
COPY_KEY=9
MOVE_TO_NEXT_WORD_KEY=32

meson compile -C "$BUILD" >/dev/null
printf 'xwayland disable\nexec bash "%s" "%s" "%s" "%s"\n' \
  "$DIR/theme-session.sh" "$BUILD" "$OUT" "$ROOT/tests/fixtures/stub" > "$OUT/sway.conf"

(
  unset WAYLAND_DISPLAY DISPLAY SWAYSOCK HYPRLAND_INSTANCE_SIGNATURE QT_QPA_PLATFORMTHEME TEXTINPUTFLOW_QT_BASE_THEME
  export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
  timeout 60 dbus-run-session -- sway -c "$OUT/sway.conf" >/dev/null 2>&1
) || true

for theme in gtk3 textinputflow; do
  if [ ! -s "$OUT/$theme.txt" ]; then
    echo "FAIL no theme dump under $theme"
    cat "$OUT/session.log" 2>/dev/null
    exit 1
  fi
done

status=0
if diff --label gtk3 --label textinputflow \
  <(grep -v '^keys ' "$OUT/gtk3.txt") <(grep -v '^keys ' "$OUT/textinputflow.txt"); then
  echo "ok   $(grep -vc '^keys ' "$OUT/gtk3.txt") theme values equal apart from keys lines"
else
  echo "FAIL theme values differ"
  status=1
fi

if [ "$(grep "^keys $COPY_KEY " "$OUT/gtk3.txt")" = "$(grep "^keys $COPY_KEY " "$OUT/textinputflow.txt")" ]; then
  echo "ok   Copy keys equal: $(grep "^keys $COPY_KEY " "$OUT/textinputflow.txt")"
else
  echo "FAIL Copy keys differ"
  status=1
fi

echo "== keys lines (gtk3, then textinputflow)"
grep '^keys ' "$OUT/gtk3.txt" "$OUT/textinputflow.txt" | sed "s|$OUT/||"
if ! grep -qx "keys $MOVE_TO_NEXT_WORD_KEY Alt+Right" "$OUT/textinputflow.txt"; then
  echo "FAIL the textinputflow plugin did not answer MoveToNextWord"
  status=1
fi
exit "$status"

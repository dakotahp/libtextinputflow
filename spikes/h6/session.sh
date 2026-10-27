#!/usr/bin/env bash
set -uo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
exec > "$1" 2> "$1.log"
trap 'swaymsg exit' EXIT

# The system Python has the gi and PyQt6 modules; a mise or venv python3 on PATH may not.
PY=/usr/bin/python3
export GSK_RENDERER=cairo GDK_BACKEND=wayland QT_QPA_PLATFORM=wayland QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1

# The accessibility bus runs on dbus-broker, which starts the registry through systemd.
# A private session bus has no systemd, so start both by hand.
/usr/lib/at-spi-bus-launcher --launch-immediately &
sleep 1
/usr/lib/at-spi2-registryd &
sleep 1

# The headless seat has no keyboard, so windows never get keyboard focus. Each wtype run adds a
# virtual keyboard and removes it on exit; one idle wtype keeps a keyboard on the seat throughout.
wtype -s 600000 &
sleep 0.5

STEPS=(
  "start:"
  "Ctrl+Right:-M ctrl -k Right -m ctrl"
  "Ctrl+Right:-M ctrl -k Right -m ctrl"
  "Shift+Ctrl+Right:-M ctrl -M shift -k Right -m shift -m ctrl"
  "Home:-k Home"
  "End:-k End"
  "Ctrl+a:-M ctrl -k a -m ctrl"
)

run_case() {
  local toolkit=$1 widget=$2 app_name=$3
  shift 3
  if [ -n "${H6_ONLY:-}" ] && [ "$H6_ONLY" != "$toolkit-$widget" ]; then
    return
  fi
  "$@" &
  local pid=$!
  if [ -n "${H6_DUMP:-}" ]; then
    sleep 3
    "$PY" "$DIR/probe.py" "$app_name" --dump >&2
  fi
  for step in "${STEPS[@]}"; do
    local label=${step%%:*} keys=${step#*:}
    if [ -n "$keys" ]; then
      # shellcheck disable=SC2086
      wtype $keys
      sleep 0.4
    fi
    local result
    result=$("$PY" "$DIR/probe.py" "$app_name" --wait 10)
    result=${result:-'{"error": "probe crashed"}'}
    printf '{"toolkit":"%s","widget":"%s","step":"%s","result":%s}\n' \
      "$toolkit" "$widget" "$label" "$result"
  done
  kill "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null
}

run_case gtk4 entry tif-gtk4 "$PY" "$DIR/gtk4_app.py" entry
run_case gtk4 textview tif-gtk4 "$PY" "$DIR/gtk4_app.py" textview
run_case qt lineedit tif-qt "$PY" "$DIR/qt_app.py" entry
run_case qt plaintextedit tif-qt "$PY" "$DIR/qt_app.py" textview

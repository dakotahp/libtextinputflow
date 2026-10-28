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
if [ -n "${H6_STEPS:-}" ]; then
  mapfile -t STEPS < "$H6_STEPS"
fi
if [ -n "${H6_QT_THEME:-}" ]; then
  export QT_QPA_PLATFORMTHEME=$H6_QT_THEME
fi
# sway has a file capability (cap_sys_nice), so glibc strips LD_LIBRARY_PATH before sway starts.
if [ -n "${H6_LD_LIBRARY_PATH:-}" ]; then
  export LD_LIBRARY_PATH=$H6_LD_LIBRARY_PATH
fi

run_case() {
  local toolkit=$1 widget=$2 app_name=$3
  shift 3
  if [ -n "${H6_ONLY:-}" ] && [[ "$toolkit-$widget" != "$H6_ONLY"* ]]; then
    return
  fi
  "$@" &
  local pid=$!
  if [ -n "${H6_DUMP:-}" ]; then
    sleep 3
    "$PY" "$DIR/probe.py" "$app_name" --dump >&2
    grep -oE '/[^ ]*lib(gtk-4|Qt6Gui|textinputflow)\.so[^ ]*' "/proc/$pid/maps" | sort -u >&2
  fi
  for step in "${STEPS[@]}"; do
    local label=${step%%:*} keys=${step#*:}
    if [ "$keys" = "@USR1" ]; then
      kill -USR1 "$pid"
      sleep 0.4
    elif [[ $keys == "@sh "* ]]; then
      bash -c "${keys#@sh }" >&2
      sleep 1
    elif [ -n "$keys" ]; then
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

GTK_APP=${H6_GTK_APP:-$DIR/gtk4_app.py}
run_case gtk4 entry tif-gtk4 "$PY" "$GTK_APP" entry
run_case gtk4 textview tif-gtk4 "$PY" "$GTK_APP" textview
QT_APP=${H6_QT_APP:-$DIR/qt_app.py}
run_case qt lineedit tif-qt "$PY" "$QT_APP" entry
run_case qt plaintextedit tif-qt "$PY" "$QT_APP" textview

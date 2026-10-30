#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../../.." && pwd)
BUILD=$(cd "${1:?usage: check-keys.sh <meson build dir>}" && pwd)
STUB=$ROOT/tests/fixtures/stub
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

meson compile -C "$BUILD" >/dev/null

summarize() {
  /usr/bin/python3 -c '
import json, sys
for line in open(sys.argv[1]):
    if not line.startswith("{"):
        continue
    row = json.loads(line)
    result = row["result"]
    if "error" in result:
        print(row["widget"], row["step"], "error:", result["error"])
        continue
    selection = ",".join(f"{start}-{end}" for start, end in result["selection"]) or "-"
    print(row["widget"], row["step"], result["caret"], selection, json.dumps(result["text"]))
' "$1"
}

status=0
check() {
  local name=$1 expected=$2
  shift 2
  env -u TEXTINPUTFLOW_QT_BASE_THEME \
    H6_ONLY=qt H6_QT_THEME=textinputflow QT_PLUGIN_PATH="$BUILD/adapters/qt" \
    XDG_CONFIG_HOME="$STUB" XDG_DATA_DIRS="$STUB:/usr/local/share:/usr/share" "$@" \
    "$ROOT/tests/harness/run.sh" "$OUT/$name.jsonl" >/dev/null 2>&1 || true
  if diff -u --label expected --label actual "$expected" <(summarize "$OUT/$name.jsonl"); then
    echo "ok   $name"
  else
    echo "FAIL $name"
    status=1
  fi
}

SHORTCUT_APPS=(H6_QT_APP="$DIR/qt_shortcut_app.py" H6_QTQUICK_APP="$DIR/qtquick_shortcut_app.py")

check keys "$DIR/keys.expected" H6_STEPS="$DIR/keys.steps"
check keys-nonexistent-base "$DIR/keys.expected" H6_STEPS="$DIR/keys.steps" TEXTINPUTFLOW_QT_BASE_THEME=nonexistent
check shortcuts "$DIR/shortcuts.expected" H6_STEPS="$DIR/shortcuts.steps" "${SHORTCUT_APPS[@]}"
check shortcuts-document-end "$DIR/shortcuts-document-end.expected" H6_STEPS="$DIR/document-end.steps" \
  "${SHORTCUT_APPS[@]}" H6_ONLY=qt-plaintextedit
check shortcuts-document-end-quick "$DIR/shortcuts-document-end-quick.expected" H6_STEPS="$DIR/document-end.steps" \
  "${SHORTCUT_APPS[@]}" H6_ONLY=qtquick-textarea
check shortcuts-read-only "$DIR/shortcuts-read-only.expected" H6_STEPS="$DIR/alt-backspace.steps" \
  "${SHORTCUT_APPS[@]}" H6_ONLY=qt-lineedit TIF_QT_READ_ONLY=1
check shortcuts-read-only-quick "$DIR/shortcuts-read-only-quick.expected" H6_STEPS="$DIR/alt-backspace.steps" \
  "${SHORTCUT_APPS[@]}" H6_ONLY=qtquick-textfield TIF_QT_READ_ONLY=1
check shortcuts-plugin-off "$DIR/shortcuts-plugin-off.expected" H6_STEPS="$DIR/alt-backspace.steps" \
  "${SHORTCUT_APPS[@]}" H6_QT_THEME=
exit "$status"

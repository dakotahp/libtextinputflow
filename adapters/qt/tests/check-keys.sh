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
    print(row["widget"], row["step"], result["caret"], selection)
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

check keys "$DIR/keys.expected" H6_STEPS="$DIR/keys.steps"
check keys-nonexistent-base "$DIR/keys.expected" H6_STEPS="$DIR/keys.steps" TEXTINPUTFLOW_QT_BASE_THEME=nonexistent
check shortcuts "$DIR/shortcuts.expected" H6_STEPS="$DIR/shortcuts.steps" H6_QT_APP="$DIR/qt_shortcut_app.py"
exit "$status"

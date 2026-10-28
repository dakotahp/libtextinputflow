#!/usr/bin/env bash
set -euo pipefail

USAGE='usage: check-keys.sh <work dir> <textinputflow build dir>'
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../../.." && pwd)
WORK=${1:?$USAGE}
TIF_BUILD=$(cd "${2:?$USAGE}" && pwd -P)
TIF_LIB=$TIF_BUILD/lib
STUB=$ROOT/tests/fixtures/stub
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

meson compile -C "$TIF_BUILD" >/dev/null

build() {
  local feature=$1
  if ! "$DIR/../build-gtk.sh" "$WORK" "$TIF_BUILD" "$feature" 2>"$OUT/build-$feature.log"; then
    tail -n 40 "$OUT/build-$feature.log" >&2
    echo "FAIL build-$feature" >&2
    exit 1
  fi
}
ENABLED=$(build enabled)
DISABLED=$(build disabled)

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
report() {
  if [ "$2" = ok ]; then echo "ok   $1"; else echo "FAIL $1"; status=1; fi
}

run() {
  local name=$1 gtk_lib=$2 steps=$3
  H6_ONLY=gtk4 H6_DUMP=1 H6_STEPS="$steps" H6_LD_LIBRARY_PATH="$gtk_lib:$TIF_LIB" \
    XDG_CONFIG_HOME="$STUB" XDG_DATA_DIRS="$STUB:/usr/local/share:/usr/share" \
    "$ROOT/spikes/h6/run.sh" "$OUT/$name.jsonl" >/dev/null 2>&1 || true
}

check_results() {
  local name=$1 expected=$2
  if diff -u --label expected --label actual "$expected" <(summarize "$OUT/$name.jsonl"); then
    report "$name" ok
  else
    report "$name" fail
  fi
}

# Each of the two test apps logs every matching library it mapped, once.
check_loaded() {
  local name=$1 library=$2 want=$3 got
  got=$(grep -cF "$library" "$OUT/$name.jsonl.log" || true)
  if [ "$got" = "$want" ]; then
    report "$name loaded $library x$want" ok
  else
    echo "expected $want apps to load $library, got $got" >&2
    report "$name loaded $library x$want" fail
  fi
}

run enabled "$ENABLED" "$DIR/keys.steps"
check_results enabled "$DIR/keys.expected"
check_loaded enabled "$ENABLED/libgtk-4.so" 2
check_loaded enabled "$TIF_LIB/libtextinputflow.so.0" 2

run disabled "$DISABLED" "$DIR/stock.steps"
check_results disabled "$DIR/stock.expected"
check_loaded disabled "$DISABLED/libgtk-4.so" 2
check_loaded disabled "$TIF_LIB/libtextinputflow.so.0" 0

exit "$status"

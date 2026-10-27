#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$DIR/build}
mkdir -p "$OUT"
"$DIR/build.sh" "$OUT" >/dev/null

H6_STEPS="$DIR/steps.txt" "$DIR/../h6/run.sh" "$OUT/default.jsonl" >/dev/null 2>&1
H6_STEPS="$DIR/steps.txt" H6_QT_THEME=tifspike QT_PLUGIN_PATH="$OUT" \
  "$DIR/../h6/run.sh" "$OUT/tifspike.jsonl" >/dev/null 2>&1

for theme in default tifspike; do
  echo "== Qt theme: $theme"
  grep '"toolkit":"qt"' "$OUT/$theme.jsonl" | sed -E \
    -e 's/\{"toolkit":"qt","widget":"([^"]*)","step":"([^"]*)","result":/\1 \2 => /' \
    -e 's/\}$//'
done

#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$DIR/build}
mkdir -p "$OUT"

H6_ONLY=gtk4 H6_STEPS="$DIR/steps.txt" H6_GTK_APP="$DIR/gtk4_controller_app.py" \
  "$DIR/../h6/run.sh" "$OUT/results.jsonl" >/dev/null 2>&1

sed -E \
  -e 's/\{"toolkit":"gtk4","widget":"([^"]*)","step":"([^"]*)","result":/\1 \2 => /' \
  -e 's/\}$//' "$OUT/results.jsonl"

#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../.." && pwd)
BUILD=${1:-$ROOT/build}

if [ ! -f "$BUILD/build.ninja" ]; then
  meson setup "$BUILD" "$ROOT" -Dqt=enabled >/dev/null
fi
BUILD=$(cd "$BUILD" && pwd -P)
WORK=${2:-$BUILD/gtk-work}
OUT=$BUILD/e2e

rm -rf "$OUT"
mkdir -p "$OUT/data/textinputflow" "$OUT/config/textinputflow"

if ! meson compile -C "$BUILD" >"$OUT/build.log" 2>&1; then
  tail -n 40 "$OUT/build.log" >&2
  echo "library build failed; full log in $OUT/build.log" >&2
  exit 1
fi
if [ ! -f "$BUILD/adapters/qt/platformthemes/libtextinputflow-qt.so" ]; then
  echo "no Qt plugin in $BUILD: run meson configure $BUILD -Dqt=enabled" >&2
  exit 1
fi

if ! GTK_LIB=$("$ROOT/adapters/gtk4/build-gtk.sh" "$WORK" "$BUILD" enabled 2>"$OUT/build-gtk.log"); then
  tail -n 40 "$OUT/build-gtk.log" >&2
  echo "GTK build failed; full log in $OUT/build-gtk.log" >&2
  exit 1
fi

cp "$ROOT/data/bindings" "$OUT/data/textinputflow/bindings"
cp "$DIR/user-v1" "$OUT/config/textinputflow/bindings"

env -u TEXTINPUTFLOW_QT_BASE_THEME -u H6_ONLY -u H6_GTK_APP -u H6_QT_APP \
  E2E="$DIR" H6_STEPS="$DIR/steps" \
  H6_LD_LIBRARY_PATH="$GTK_LIB:$BUILD/lib" \
  H6_QT_THEME=textinputflow QT_PLUGIN_PATH="$BUILD/adapters/qt" \
  XDG_CONFIG_HOME="$OUT/config" XDG_DATA_DIRS="$OUT/data:/usr/local/share:/usr/share" \
  "$ROOT/tests/harness/run.sh" "$OUT/results.jsonl" >/dev/null 2>&1 || true

if ! /usr/bin/python3 "$DIR/compare.py" "$DIR/expected.jsonl" "$OUT/results.jsonl"; then
  echo "results: $OUT/results.jsonl, harness log: $OUT/results.jsonl.log" >&2
  exit 1
fi

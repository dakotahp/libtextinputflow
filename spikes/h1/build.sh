#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$DIR/build}
QT_VERSION=$(pkg-config --modversion Qt6Gui)

INCLUDES=(
  -I"$OUT" -I"$DIR/theme"
  -I"/usr/include/qt6/QtGui/$QT_VERSION" -I"/usr/include/qt6/QtCore/$QT_VERSION"
  -I/usr/include/qt6 -I/usr/include/qt6/QtGui -I/usr/include/qt6/QtCore
)

mkdir -p "$OUT/platformthemes"
/usr/lib/qt6/moc "${INCLUDES[@]}" "$DIR/theme/tifspike.cpp" -o "$OUT/tifspike.moc"

# shellcheck disable=SC2046
g++ -std=c++17 -shared -fPIC -O1 \
  "${INCLUDES[@]}" \
  $(pkg-config --cflags Qt6Gui) \
  "$DIR/theme/tifspike.cpp" \
  $(pkg-config --libs Qt6Gui) \
  -o "$OUT/platformthemes/libtifspike.so"

echo "$OUT"

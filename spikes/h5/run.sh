#!/usr/bin/env bash
set -euo pipefail

DIR=$(cd "$(dirname "$0")" && pwd)
WORK=${1:-${TMPDIR:-/tmp}/tif-gtk-h5}
VERSION=4.24.1
URL=https://download.gnome.org/sources/gtk/${VERSION%.*}/gtk-$VERSION.tar.xz
SRC=$WORK/gtk-$VERSION
BUILD=$WORK/build
mkdir -p "$WORK"

if [ ! -f "$WORK/gtk-$VERSION.tar.xz" ]; then
  curl -sfL -o "$WORK/gtk-$VERSION.tar.xz" "$URL"
  curl -sfL -o "$WORK/gtk-$VERSION.sha256sum" "${URL%.tar.xz}.sha256sum"
fi
(cd "$WORK" && sha256sum -c --ignore-missing --quiet "gtk-$VERSION.sha256sum")

if [ ! -d "$SRC" ]; then
  tar -xJf "$WORK/gtk-$VERSION.tar.xz" -C "$WORK"
  patch -d "$SRC" -p1 < "$DIR/marker.patch"
fi

start=$(date +%s)
"$DIR/build-gtk.sh" "$SRC" "$BUILD" > "$WORK/build.log" 2>&1
echo "build: $(( $(date +%s) - start ))s, build dir $(du -sh "$BUILD" | cut -f1), source dir $(du -sh "$SRC" | cut -f1)"

LIB_DIR=$BUILD/gtk
echo "== libgtk-4 loaded by Python with LD_LIBRARY_PATH=$LIB_DIR"
LD_LIBRARY_PATH=$LIB_DIR /usr/bin/python3 -I -c \
  'import gi; gi.require_version("Gtk", "4.0"); from gi.repository import Gtk; print(Gtk.get_micro_version()); print(open("/proc/self/maps").read())' \
  | grep -m1 -oE '/[^ ]*libgtk-4\.so[^ ]*'
echo "== libgtk-4 loaded without it"
/usr/bin/python3 -I -c \
  'import gi; gi.require_version("Gtk", "4.0"); from gi.repository import Gtk; print(open("/proc/self/maps").read())' \
  | grep -m1 -oE '/[^ ]*libgtk-4\.so[^ ]*'

for gtk in system built; do
  if [ "$gtk" = built ]; then export H6_LD_LIBRARY_PATH=$LIB_DIR; else unset H6_LD_LIBRARY_PATH; fi
  H6_ONLY=gtk4 H6_DUMP=1 H6_STEPS="$DIR/steps.txt" "$DIR/../h6/run.sh" "$WORK/$gtk.jsonl" >/dev/null 2>&1
  echo "== caret with $gtk GTK, loaded: $(grep -oE '/[^ ]*libgtk-4\.so[^ ]*' "$WORK/$gtk.jsonl.log" | sort -u | tr '\n' ' ')"
  sed -E \
    -e 's/\{"toolkit":"gtk4","widget":"([^"]*)","step":"([^"]*)","result":/\1 \2 => /' \
    -e 's/\}$//' "$WORK/$gtk.jsonl" | grep -v SpiRegistry
done

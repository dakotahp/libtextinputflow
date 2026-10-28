#!/usr/bin/env bash
set -euo pipefail

USAGE='usage: build-gtk.sh <work dir> <textinputflow build dir> [enabled|disabled]'
DIR=$(cd "$(dirname "$0")" && pwd)
WORK=${1:?$USAGE}
TIF_BUILD=$(cd "${2:?$USAGE}" && pwd -P)
FEATURE=${3:-enabled}
case $FEATURE in
  enabled | disabled) ;;
  *) echo "$USAGE" >&2; exit 2 ;;
esac

mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd -P)
VERSION=4.24.1
URL=https://download.gnome.org/sources/gtk/${VERSION%.*}/gtk-$VERSION.tar.xz
SRC=$WORK/gtk-$VERSION
BUILD=$WORK/build-$FEATURE
PATCH=$DIR/textinputflow.patch
APPLIED=$WORK/applied.patch

# Homebrew and mise put their own glib tools and python3 first on PATH; GTK must use the system ones.
PATH=$(tr ':' '\n' <<< "$PATH" | grep -vE 'linuxbrew|mise' | paste -sd:)
PKG_CONFIG_PATH=$TIF_BUILD/meson-uninstalled${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
export PATH PKG_CONFIG_PATH

if [ ! -f "$WORK/gtk-$VERSION.tar.xz" ]; then
  curl -sfL -o "$WORK/gtk-$VERSION.tar.xz.part" "$URL"
  mv "$WORK/gtk-$VERSION.tar.xz.part" "$WORK/gtk-$VERSION.tar.xz"
fi
if [ ! -f "$WORK/gtk-$VERSION.sha256sum" ]; then
  curl -sfL -o "$WORK/gtk-$VERSION.sha256sum" "${URL%.tar.xz}.sha256sum"
fi
(cd "$WORK" && sha256sum -c --ignore-missing --quiet --strict "gtk-$VERSION.sha256sum") >&2
grep -q " gtk-$VERSION.tar.xz\$" "$WORK/gtk-$VERSION.sha256sum"

if [ ! -d "$SRC" ]; then
  rm -f "$APPLIED"
  tar -xJf "$WORK/gtk-$VERSION.tar.xz" -C "$WORK"
fi

apply() {
  if ! patch -d "$SRC" -p1 --quiet --batch --forward "$@" >&2; then
    rm -rf "$SRC" "$APPLIED"
    echo "patch failed; removed $SRC, run again to start from a fresh tarball" >&2
    exit 1
  fi
}

# Swapping one applied patch for another keeps untouched files' timestamps, so rebuilds stay small.
if ! cmp -s "$PATCH" "$APPLIED"; then
  if [ -f "$APPLIED" ]; then
    apply -R < "$APPLIED"
    rm "$APPLIED"
  fi
  apply < "$PATCH"
  cp "$PATCH" "$APPLIED.part"
  mv "$APPLIED.part" "$APPLIED"
fi

OPTIONS=(
  --buildtype=debugoptimized
  -Dwayland-backend=true
  -Dx11-backend=false
  -Dbroadway-backend=false
  -Dmedia-gstreamer=disabled
  -Dprint-cpdb=disabled
  -Dprint-cups=disabled
  -Dvulkan=disabled
  -Dcloudproviders=disabled
  -Dsysprof=disabled
  -Dtracker=disabled
  -Dcolord=disabled
  -Daccesskit=disabled
  -Dintrospection=disabled
  -Ddocumentation=false
  -Dscreenshots=false
  -Dman-pages=false
  -Dbuild-demos=false
  -Dbuild-testsuite=false
  -Dbuild-examples=false
  -Dbuild-tests=false
  "-Dtextinputflow=$FEATURE"
)

if [ ! -d "$BUILD" ]; then
  meson setup "$BUILD" "$SRC" "${OPTIONS[@]}" >&2
fi
meson compile -C "$BUILD" >&2
echo "$BUILD/gtk"

#!/usr/bin/env bash
set -euo pipefail

SRC=${1:?usage: build-gtk.sh <gtk source dir> <build dir> [setup|compile]}
BUILD=${2:?usage: build-gtk.sh <gtk source dir> <build dir> [setup|compile]}
STEP=${3:-compile}

# Homebrew and mise put their own glib tools and python3 first on PATH; GTK must use the system ones.
PATH=$(tr ':' '\n' <<< "$PATH" | grep -vE 'linuxbrew|mise' | paste -sd:)
export PATH

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
)

if [ ! -d "$BUILD" ]; then
  meson setup "$BUILD" "$SRC" "${OPTIONS[@]}"
fi
if [ "$STEP" = compile ]; then
  meson compile -C "$BUILD"
fi

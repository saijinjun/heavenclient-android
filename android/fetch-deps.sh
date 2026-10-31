#!/usr/bin/env bash
# Downloads the native libraries the Android build compiles from source into
# android/third_party. Safe to re-run: existing copies are kept.
set -euo pipefail

SDL2_VERSION=2.30.9
FREETYPE_VERSION=2.13.3
ASIO_VERSION=1-30-2

cd "$(dirname "$0")"
mkdir -p third_party
cd third_party

fetch() {
    local name="$1" url="$2"
    if [[ -d "$name" ]]; then
        echo "$name already present"
        return
    fi
    echo "Downloading $name"
    curl -fsSL "$url" -o "$name.tar.gz"
    mkdir "$name"
    tar -xzf "$name.tar.gz" -C "$name" --strip-components=1
    rm "$name.tar.gz"
}

fetch SDL2 "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz"
fetch freetype "https://github.com/freetype/freetype/archive/refs/tags/VER-${FREETYPE_VERSION//./-}.tar.gz"
fetch asio "https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-${ASIO_VERSION}.tar.gz"
